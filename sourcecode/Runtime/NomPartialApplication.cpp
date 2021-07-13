#include "NomPartialApplication.h"
#include "RTPartialApp.h"
#include <unordered_map>
#include "RTSubtyping.h"
#include "CompileHelpers.h"
#include "llvm/Support/raw_os_ostream.h"
#include <iostream>
#include "llvm/IR/Verifier.h"
#include "RTOutput.h"
#include "NomNameRepository.h"
#include "RTDescriptor.h"
#include "TypeOperations.h"
#include "RTCast.h"
#include "NomDynamicType.h"
#include "NomTypeVar.h"
#include "NomMemberContext.h"
#include "NomTypeParameter.h"
#include "IntClass.h"
#include "FloatClass.h"
#include "CallingConvConf.h"

using namespace std;
using namespace llvm;
namespace Nom
{
	namespace Runtime
	{
		class NomPartialApplicationDispatcherEnv : public ACompileEnv
		{
		public:
			const NomCallable* const Callable;
			NomPartialApplicationDispatcherEnv(RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, /*const std::vector<PhiNode*>* phiNodes, */ llvm::ArrayRef<NomTypeParameterRef> typeParams, const TypeList argtypes, const NomCallable* method, NomTypeRef thisType) : ACompileEnv(regcount, contextName, function, nullptr, typeParams, method, argtypes, thisType), Callable(method)
			{

			}
			// Inherited via CompileEnv
			virtual NomTypeVarValue GetTypeArgument(NomBuilder& builder, int i) override
			{
				if ((size_t)i < Callable->GetTypeParametersStart())
				{
					return NomTypeVarValue(ObjectHeader::GenerateReadTypeArgument(builder, registers[0], i + Callable->GetParent()->GetTypeParametersStart()), Context->GetTypeParameter(i)->GetVariable());
					//	builder->CreateGEP(thisObj, { MakeInt<int32_t>(0), MakeInt<int32_t>(), MakeInt<int32_t>(i + envTypeArgStart) }), Context->GetTypeVariable(i));
				}
				else
				{
					return TypeArguments[i - Callable->GetTypeParametersStart()];
				}
			}
			virtual size_t GetEnvTypeArgumentCount() override
			{
				return Callable->GetTypeParametersCount() - Callable->GetDirectTypeParametersCount();
			}
			virtual llvm::Value* GetEnvTypeArgumentArray(NomBuilder& builder) override
			{
				return builder->CreateGEP(ObjectHeader::GeneratePointerToTypeArguments(builder, registers[0]), MakeInt32((uint32_t)Callable->GetParent()->GetTypeParametersStart()));
			}
		};
		class NomPartialApplicationDispatcherEntry : public NomCallable, public NomMemberContextInternal
		{
		private:
			const std::string name;
			const std::string qname;
			int32_t argCount;
			llvm::ArrayRef<const NomCallable*> overloadings;
			NomTypeRef thisType;
			TypeList argTypes;
		public:
			NomPartialApplicationDispatcherEntry(const NomMemberContext* parent, llvm::ArrayRef<const NomCallable*> overloadings, size_t typeArgCount, int32_t argCount, NomTypeRef thisType) : NomCallable(), NomMemberContextInternal(parent), name("NOM_OD_" + overloadings[0]->GetName() + "$$$" + to_string(typeArgCount) + "$" + to_string(argCount)), qname("NOM_OD_" + overloadings[0]->GetName() + "$$$" + to_string(typeArgCount) + "$" + to_string(argCount)), argCount(argCount), overloadings(overloadings), thisType(thisType)
			{
				NomTypeParameterRef* typeArgBuf = makenmalloc(NomTypeParameterRef, typeArgCount);
				for (decltype(typeArgCount) i = 0; i < typeArgCount; i++)
				{
					typeArgBuf[i] = new NomTypeParameterInternal(parent, i, NomType::AnythingRef, NomType::NothingRef);
				}
				NomMemberContextInternal::SetDirectTypeParameters(notnullarray(NomTypeParameterRef, typeArgBuf, typeArgCount));

				NomTypeRef* argBuf = makenmalloc(NomTypeRef, argCount);
				for (decltype(argCount) i = 0; i < argCount; i++)
				{
					argBuf[i] = NomType::DynamicRef;
				}
				argTypes = TypeList(argBuf, argCount);
			}
			// Inherited via NomCallable
			virtual Function* createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const override
			{
				auto typeParams = GetDirectTypeParameters();
				decltype(typeParams.size()) typeArgCount = typeParams.size();
				NomBuilder builder;
				std::string funname = "NOM_OD_" + overloadings[0]->GetName() + "$$$" + to_string(typeArgCount) + "$" + to_string(argCount);
				Function* dispatcher = Function::Create(NomPartialApplication::GetDynamicDispatcherType(typeArgCount, argCount), linkage, funname, &mod);
				dispatcher->setCallingConv(NOMCC);

				NomTypeRef* argTypesArr = makealloca(NomTypeRef, argCount);
				for (decltype(argCount) i = 0; i < argCount; i++)
				{
					argTypesArr[i] = &NomDynamicType::Instance();
				}

				NomPartialApplicationDispatcherEnv padenv(argCount + 1, funname, dispatcher, typeParams, TypeList(argTypesArr, argCount), this, thisType);
				CompileEnv* env = &padenv;

				//auto dictEntry = new GlobalVariable(mod, GetDescriptorDictionaryEntryType(), true, linkage, CreateDescriptorDictionaryEntryConstant(DescriptorDictionaryEntryKind::Dispatcher, true, Visibility::Public, dispatcher, 0));

				BasicBlock* dispblock = BasicBlock::Create(LLVMCONTEXT, "", dispatcher);
				builder->SetInsertPoint(dispblock);
				llvm::Value* targalloca;
				if (typeArgCount > 0)
				{
					targalloca = builder->CreateAlloca(RTSubtyping::TypeArgumentListStackType());
					auto argfields = builder->CreateAlloca(TYPETYPE, MakeInt<uint32_t>(typeArgCount));
					MakeStore(builder, mod, ConstantPointerNull::get(RTSubtyping::TypeArgumentListStackType()->getPointerTo()), builder->CreateGEP(targalloca, { MakeInt32(0), MakeInt32(TypeArgumentListStackFields::Next) }));
					MakeStore(builder, mod, argfields, builder->CreateGEP(targalloca, { MakeInt32(0), MakeInt32(TypeArgumentListStackFields::Types) }));
					auto disparg = dispatcher->arg_begin();
					for (decltype(typeArgCount) i = 0; i < typeArgCount; i++, disparg++)
					{
						MakeStore(builder, mod, disparg, builder->CreateGEP(argfields, MakeInt32((int32_t)i)));
					}
				}
				else
				{
					targalloca = llvm::ConstantPointerNull::get(RTSubtyping::TypeArgumentListStackType()->getPointerTo());
				}
				llvm::Value** methodargs = makealloca(llvm::Value*, (size_t)typeArgCount + 1 + argCount);
				llvm::Value** typeargtypes = makealloca(llvm::Value*, typeArgCount);
				//llvm::Value** valargtypes = makealloca(llvm::Value*, argCount);
				llvm::Value** valargs = makealloca(llvm::Value*, argCount);
				auto disparg = dispatcher->arg_begin();
				//we know the this-pointer is ok; that's where we got the dispatcher from in the first place
				methodargs[0] = disparg;
				disparg++;
				for (decltype(typeArgCount) i = 0; i < typeArgCount; i++, disparg++)
				{
					typeargtypes[i] = disparg;
					methodargs[i+1] = disparg;
				}
				for (decltype(argCount) i = 0; i < argCount; i++, disparg++)
				{
					//valargtypes[i] = ObjectHeader::CreateExtractType(builder, mod, disparg);
					valargs[i] = disparg;
					methodargs[i + typeArgCount + 1] = disparg;
				}

				NomSubstitutionContextMemberContext nscmc(this);

				for (auto& meth : overloadings)
				{
					BasicBlock* nextMethodBlock = BasicBlock::Create(LLVMCONTEXT, "", dispatcher);
					for (decltype(typeArgCount) i = 0; i < typeArgCount; i++)
					{
						throw new std::exception(); //TODO: implement checking type argument constraints
					}
					auto methargtypes = meth->GetArgumentTypes(&nscmc);
					for (decltype(argCount) i = 0; i < argCount; i++)
					{
						/*llvm::Value* agg = RTSubtyping::CreateTypeSubtypingCheck(builder, mod, valargtypes[i], methargtypes[i]->GetLLVMElement(mod), ConstantPointerNull::get(
						eArgumentListStackType()->getPointerTo()), targalloca);*/
						llvm::Value* agg = RTCast::GenerateCast(builder, env, valargs[i], methargtypes[i]);
						auto nextBlock = BasicBlock::Create(LLVMCONTEXT, "", dispatcher);
						builder->CreateCondBr(builder->CreateICmpEQ(agg, MakeInt(1, (uint64_t)0)), nextMethodBlock, nextBlock);
						builder->SetInsertPoint(nextBlock);
					}
					auto fcargs = ArrayRef<llvm::Value*>(methodargs, (size_t)typeArgCount + 1 + argCount);
					auto methcall = GenerateFunctionCall(builder, mod, meth->GetLLVMElement(mod), fcargs, true);
					if (methcall->getCallingConv() == llvm::CallingConv::Fast)
					{
						methcall->setTailCallKind(llvm::CallInst::TailCallKind::TCK_Tail);
					}
					auto mctype = methcall->getType();
					llvm::Value* retval = methcall;
					if (mctype->isIntegerTy(1))
					{
						retval = PackBool(builder, methcall);
					}
					else if (mctype->isIntegerTy(INTTYPE->getPrimitiveSizeInBits()))
					{
						retval = PackInt(builder, methcall);
					}
					else if (mctype->isFloatingPointTy())
					{
						retval = PackFloat(builder, methcall);
					}
					builder->CreateRet(retval);
					builder->SetInsertPoint(nextMethodBlock);
				}
				static const char* failstr = "Could not find applicable method!";
				builder->CreateRet(builder->CreateCall(RTOutput_Fail::GetLLVMElement(mod), { GetLLVMPointer(failstr) }));

				llvm::raw_os_ostream out(std::cout);
				if (verifyFunction(*dispatcher, &out))
				{
					out.flush();
					std::cout << "Could not verify Dispatcher for method name ";
					std::cout << overloadings[0]->GetName();
					dispatcher->print(out);
					out.flush();
					std::cout.flush();
					throw new std::exception();
				}
				return dispatcher;
			}
			//virtual NomTypeRef GetReturnType() const override
			//{
			//	return &NomDynamicType::Instance();
			//}
			//virtual TypeList GetArgumentTypes() const override
			//{
			//	return TypeList();
			//}
			virtual int GetArgumentCount() const override
			{
				return argCount;
			}

			// Inherited via NomCallable
			virtual NomTypeRef GetReturnType(const NomSubstitutionContext* context) const override
			{
				NomTypeRef ret = nullptr;
				for (auto ovl : overloadings)
				{
					if (ret == nullptr)
					{
						ret = ovl->GetReturnType(context);
					}
					else
					{
						ret = JoinTypes(ret,ovl->GetReturnType(context));
					}
				}
				return ret;
			}
			virtual TypeList GetArgumentTypes(const NomSubstitutionContext* context) const override
			{
				return argTypes;
			}
			virtual llvm::FunctionType* GetLLVMFunctionType(const NomSubstitutionContext* context = nullptr) const override
			{
				std::vector<llvm::Type* > args(GetDirectTypeParametersCount() + GetArgumentCount() + 1);
				unsigned int j = 0;
				if (GetParent() == NomIntClass::GetInstance())
				{
					args[j] = INTTYPE;
				}
				else if (GetParent() == NomFloatClass::GetInstance())
				{
					args[j] = FLOATTYPE;
				}
				else
				{
					args[j] = REFTYPE;
				}
				for (j = 1; j <= GetDirectTypeParametersCount(); j++)
				{
					args[j] = TYPETYPE;
				}
				unsigned int i;
				auto argtypes = (GetArgumentTypes(context));
				for (i = 0; i < GetArgumentCount(); i++)
				{
					args[j + i + 1] = argtypes[i]->GetLLVMType();
				}
				return llvm::FunctionType::get(GetReturnType(context)->GetLLVMType(), args, false);
			}
			virtual const std::string& GetName() const override
			{
				return name;
			}
			virtual const std::string& GetQName() const override
			{
				return qname;
			}
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override
			{
				return GetDirectTypeParameters();
			}
};
		NomPartialApplication::NomPartialApplication(const std::string symbolName, llvm::ArrayRef<const NomCallable*> methods, const NomMemberContext* context, NomTypeRef thisType) : /*NomDescriptor("RT_NOM_PA_DICT_" + symbolName),*/  context(context), thisType(thisType), SymbolName("RT_NOM_PA_" + symbolName)
		{
			for (const NomCallable* meth : methods)
			{
				this->methods.push_back(meth);
			}
		}
		llvm::FunctionType* NomPartialApplication::GetDynamicDispatcherType(uint32_t typeargcount, uint32_t argcount)
		{
			llvm::Type** argtypes = makealloca(llvm::Type*, (size_t)typeargcount + argcount + 1);
			uint32_t pos = 0;
			argtypes[pos] = REFTYPE;
			pos++;
			for (; pos < typeargcount; pos++)
			{
				argtypes[pos] = TYPETYPE;
			}
			for (; pos < typeargcount + argcount + 1; pos++)
			{
				argtypes[pos] = REFTYPE;
			}
			return llvm::FunctionType::get(REFTYPE, llvm::ArrayRef<llvm::Type*>(argtypes, (size_t)typeargcount + argcount + 1), false);
		}

		llvm::Function* NomPartialApplication::GetDispatcherEntry(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage, int32_t typeArgCount, int32_t argCount, llvm::ArrayRef<const NomCallable*> overloadings, const NomMemberContext* context, NomTypeRef thisType)
		{
			return ((new NomPartialApplicationDispatcherEntry(context, overloadings, typeArgCount, argCount, thisType))->createLLVMElement(mod, linkage));
		}

		llvm::Constant* NomPartialApplication::createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const
		{
			unordered_map<uint32_t, unordered_map<uint32_t, vector<const NomCallable*>>> overloadings;

			int32_t dispcount = 0;

			for (const NomCallable* meth : methods)
			{
				uint32_t dtac = meth->GetDirectTypeParametersCount();
				auto tamatch = overloadings.find(dtac);
				if (tamatch == overloadings.end())
				{
					overloadings[dtac] = unordered_map<uint32_t, vector<const NomCallable* >>();
				}
				auto& ole = overloadings[dtac];
				uint32_t tpc = meth->GetArgumentCount();
				auto argmatch = ole.find(tpc);
				if (argmatch == ole.end())
				{
					ole[tpc] = vector<const NomCallable*>();
					dispcount++;
				}
				ole[tpc].push_back(meth);
			}

			using npa_arg = pair<pair<uint32_t, uint32_t>, Constant*>;
			auto argsbuf = makealloca(npa_arg, dispcount);
			int32_t constantsBufPos = 0;

			for (auto& ole1 : overloadings)
			{
				for (auto& ole2 : ole1.second)
				{
					argsbuf[constantsBufPos] = make_pair(make_pair(ole1.first, ole2.first), ConstantExpr::getPointerCast(GetDispatcherEntry(mod, linkage, ole1.first, ole2.first, ole2.second, context, thisType), POINTERTYPE));
					constantsBufPos++;

					//DICTKEYTYPE entryKey = NomNameRepository::Instance().GetDispatchID(ole1.first, ole2.first);
					//dictionary->AddEntryKey(entryKey);
				}
			}
			auto cnst = RTPartialApp::CreateConstant(llvm::ArrayRef<npa_arg>(argsbuf, dispcount));
			llvm::GlobalVariable* gv = new llvm::GlobalVariable(mod, cnst->getType(), true, linkage, cnst, SymbolName);
			//auto dgvtype = arrtype(GetDescriptorDictionaryEntryType()->getPointerTo(), dispcount);
			//GlobalVariable* dgv = new GlobalVariable(mod, dgvtype, true, linkage, llvm::ConstantArray::get(dgvtype, llvm::ArrayRef<llvm::Constant*>(constantsBuf, dispcount)), dictionary->SymbolName);

			return ConstantExpr::getPointerCast(gv, RTPartialApp::GetLLVMType()->getPointerTo());
		}
		llvm::Constant* NomPartialApplication::findLLVMElement(llvm::Module& mod) const
		{
			auto gv = mod.getGlobalVariable(SymbolName);
			if (gv == nullptr)
			{
				return gv;
			}
			return ConstantExpr::getPointerCast(gv, RTPartialApp::GetLLVMType()->getPointerTo());
		}
	}
}