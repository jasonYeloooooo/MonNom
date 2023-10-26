#include "NomInstance.h"
#include "NomClass.h"
#include "NomType.h"
#include "NomClassType.h"
#include "llvm/ADT/ArrayRef.h"
#include "NomMemberContext.h"
#include "NomTypeVar.h"
#include "llvm/IR/Verifier.h"
#include "IntClass.h"
#include "FloatClass.h"
#include <iostream>
#include "llvm/Support/raw_os_ostream.h"
#include "instructions/PhiNode.h"
#include "StringClass.h"
#include "ObjectClass.h"
#include "CallingConvConf.h"
#include "instructions/CallInstance.h"


namespace Nom
{
	namespace Runtime
	{
		std::string instancePrefix = "RT_NOM_Instance_";
		
		NomInstance::NomInstance() : GloballyNamed(&instancePrefix)
		{

		}
		NomInstanceLoaded::NomInstanceLoaded(const NomClass* cls, const std::string& name, const std::string& qname, const ConstantID arguments, const RegIndex regcount, const ConstantID typeArgs, bool declOnly, bool cppWrapper) : NomInstance(), NomCallableLoaded(name, cls, qname, regcount, typeArgs, arguments, declOnly, cppWrapper), /*argumentTypes(arguments),*/ returnTypeBuf(nullptr), Class(cls)
		{
		}


		NomInstanceLoaded::~NomInstanceLoaded()
		{
		}
		llvm::Function* NomInstanceLoaded::createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const
		{
			NomBuilder builder;
			if (cppWrapper)
			{
				auto funtype = GetLLVMFunctionType();
				auto wfun = Function::Create(funtype, linkage, *GetSymbolName(), &mod);
				wfun->setCallingConv(NOMCC);
				BasicBlock* forwardblock = BasicBlock::Create(LLVMCONTEXT, name + "start", wfun);
				builder->SetInsertPoint(forwardblock);
				auto ufun = Function::Create(funtype, (declOnly ? llvm::GlobalValue::LinkageTypes::ExternalLinkage : linkage), qname, &mod);
				ufun->setCallingConv(llvm::CallingConv::C);

				llvm::Value** argbuf = makealloca(llvm::Value*, funtype->getNumParams());
				auto fnumparams = funtype->getNumParams();
				for (decltype(fnumparams) i = 0; i < fnumparams; i++)
				{
					argbuf[i] = wfun->arg_begin() + i;
				}

				auto wcall = builder->CreateCall(ufun, llvm::ArrayRef<llvm::Value*>(argbuf, funtype->getNumParams()));
				wcall->setCallingConv(llvm::CallingConv::C);
				builder->CreateRet(wcall);
				return wfun;
			}

			if (declOnly)
			{
				linkage = llvm::GlobalValue::LinkageTypes::ExternalLinkage;
			}
			auto fun = llvm::Function::Create(GetLLVMFunctionType(), linkage, *GetSymbolName(), &mod);
			fun->setCallingConv(NOMCC);
			if (declOnly)
			{
				return fun;
			}
			NomClassTypeRef selfType;
			if (Class->GetTypeParametersCount() > 0)
			{
				selfType = Class->GetType(Class->GetAllTypeVariables());
			}
			else
			{
				selfType = Class->GetType();
			}

			InstanceCompileEnv cenv =  InstanceCompileEnv(regcount, *GetSymbolName(), fun, &(this->phiNodes), this->GetAllTypeParameters(), this->GetArgumentTypes(nullptr), selfType, this);
			CompileEnv* env = &cenv;


			BasicBlock* startBlock = BasicBlock::Create(LLVMCONTEXT, *GetSymbolName() + "$start", fun);
			InitializePhis(builder, fun, env);


			builder->SetInsertPoint(startBlock);

			size_t offset = Class->GetTypeArgOffset();
			for (size_t pos = 0; pos < this->Class->GetTypeParametersCount(); pos++)
			{
				ObjectHeader::GenerateWriteTypeArgument(builder, (*env)[0], offset + pos, env->GetTypeArgument(builder, pos));
			}

			auto insInstructions = this->Instructions;
			for (auto instruction : insInstructions)
			{
				instruction->Compile(builder, env, 0);
			}

			auto superclass = Class->GetSuperClass();
			if (superclass.HasElem() && superclass.Elem != NomObjectClass::GetInstance())
			{
				NomValue* superArgsBuf = (NomValue*)(nmalloc(sizeof(NomValue) * superInstanceArgs.size()));
				auto scasize = superInstanceArgs.size();
				for (decltype(scasize) i = 0; i < scasize; i++)
				{
					superArgsBuf[i] = (*env)[superInstanceArgs[i]];
				}
				superclass.Elem->GenerateInstanceCall(builder, env, superclass.TypeArgs, (*env)[0], llvm::ArrayRef<NomValue>(superArgsBuf, superInstanceArgs.size()));
			}
			cenv.SetPastInitialSetup();
			auto instructions = GetInstructions();
#ifdef INSTRUCTIONMESSAGES
			auto dbgfun = GetDebugPrint(&mod);
#endif
			for (size_t i = 0; i < instructions->size(); i++)
			{
				(*instructions)[i]->Compile(builder, env, i);
#ifdef INSTRUCTIONMESSAGES
				if (!env->basicBlockTerminated)
				{
					builder->CreateCall(dbgfun, { { GetLLVMPointer(this->GetSymbolName()->data()), MakeInt<int64_t>(i)} });
				}
#endif
			}
			builder->CreateRet((*env)[0]);
			//For some reason, verifyFunction is supposed to return false if there are no problems
			llvm::raw_os_ostream out(std::cout);
			if (verifyFunction(*fun, &out))
			{
				std::cout << "Could not verify instance!";
				fun->print(out);
				out.flush();
				std::cout.flush();
				throw name;
			}
			return fun;
		}
		NomTypeRef NomInstanceLoaded::GetReturnType(const NomSubstitutionContext* context) const
		{
			if (context != nullptr && context->GetTypeArgumentCount() > 0)
			{
				return Class->GetType(context->GetTypeParameters());
			}
			else
			{
				if (returnTypeBuf == nullptr)
				{
					NomSubstitutionContextMemberContext nscmc = NomSubstitutionContextMemberContext(this);
					returnTypeBuf = Class->GetType(nscmc.GetTypeParameters());
				}
				return returnTypeBuf;
			}
		}

		llvm::ArrayRef<NomTypeParameterRef> NomInstanceLoaded::GetArgumentTypeParameters() const
		{
			return GetAllTypeParameters();
		}

		llvm::FunctionType* NomInstance::GetLLVMFunctionType(const NomSubstitutionContext* context) const
		{
			std::vector<llvm::Type* > args(GetTypeParametersCount() + GetArgumentCount() + 1);
			unsigned int j = 0;
			if (GetClass() == NomIntClass::GetInstance())
			{
				args[j] = INTTYPE;
			}
			else if (GetClass() == NomFloatClass::GetInstance())
			{
				args[j] = FLOATTYPE;
			}
			else
			{
				args[j] = REFTYPE;
			}
			for (j = 1; j <= GetTypeParametersCount(); j++)
			{
				args[j] = TYPETYPE;
			}
			unsigned int i;
			auto argtypes = GetArgumentTypes(context);
			for (i = 0; i < argtypes.size(); i++)
			{
				args[j + i] = argtypes[i]->GetLLVMType();
			}
			return llvm::FunctionType::get(GetReturnType(context)->GetLLVMType(), args, false);
		}
		
		NomInstanceInternal::NomInstanceInternal(const std::string& qname, const NomClass* cls) : NomCallableInternal("CONSTRUCT$", qname, cls), Container(cls)
		{
		}
		NomInstanceInternal::~NomInstanceInternal()
		{
		}
		llvm::Function* NomInstanceInternal::createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const
		{
			NomBuilder builder;
			auto funtype = GetLLVMFunctionType();
			auto wfun = Function::Create(funtype, linkage, *GetSymbolName(), &mod);
			wfun->setCallingConv(NOMCC);
			BasicBlock* forwardblock = BasicBlock::Create(LLVMCONTEXT, Container->GetName()->ToStdString() + "$CONSTRUCT$start", wfun);
			builder->SetInsertPoint(forwardblock);
			auto ufun = Function::Create(funtype, linkage, GetQName(), &mod);
			ufun->setCallingConv(llvm::CallingConv::C);

			llvm::Value** argbuf = makealloca(llvm::Value*, funtype->getNumParams());
			auto fnumparams = funtype->getNumParams();
			for (decltype(fnumparams) i = 0; i < fnumparams; i++)
			{
				argbuf[i] = wfun->arg_begin() + i;
			}

			auto wcall = builder->CreateCall(ufun, llvm::ArrayRef<llvm::Value*>(argbuf, funtype->getNumParams()));
			wcall->setCallingConv(llvm::CallingConv::C);
			builder->CreateRet(wcall);

			if (linkage == llvm::GlobalValue::LinkageTypes::ExternalLinkage)
			{
				GenerateCInstance(mod, this);
			}
			return wfun;
		}
		NomTypeRef NomInstanceInternal::GetReturnType(const NomSubstitutionContext* context) const
		{
			if (context == nullptr)
			{
				return Container->GetType(GetAllTypeVariables());
			}
			return Container->GetType(context->GetTypeParameters());
		}
		llvm::ArrayRef<NomTypeParameterRef> NomInstanceInternal::GetArgumentTypeParameters() const
		{
			return GetAllTypeParameters();
		}
		const NomClass* NomInstanceInternal::GetClass() const
		{
			return Container;
		}
	}
}