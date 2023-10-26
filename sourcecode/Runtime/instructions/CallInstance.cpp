#include "CallInstance.h"
#include "../NomAlloc.h"
#include "../NomInstance.h"
#include "../NomInstantiationRef.h"
#include "../NomClass.h"
#include "../NomVMInterface.h"
#include "../NomClassType.h"
#include "../CompileHelpers.h"
#include <iostream>
#include "llvm/Support/raw_os_ostream.h"
#include "../CallingConvConf.h"
#include "llvm/IR/Verifier.h"
#include "../RTCompileConfig.h"
#include "../RefValueHeader.h"

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{

		CallInstance::CallInstance(const RegIndex reg, ConstantID cls, ConstantID typeArgs) : NomValueInstruction(reg, OpCode::CallInstance), ClassSuperClass(cls), TypeArgs(typeArgs)
		{

		}


		CallInstance::~CallInstance()
		{
		}

		llvm::Function* GenerateCInstance(llvm::Module& mod, const NomInstance* cnstrins)
		{
			auto cls = cnstrins->GetClass();

			FunctionType* ft = cnstrins->GetLLVMFunctionType();

			auto targcount = cnstrins->GetTypeParametersCount();
			

			auto argcount = cnstrins->GetArgumentCount();
			int i;
			llvm::Type** argtarr = makealloca(llvm::Type*, targcount + argcount);
			for (i = 1; i <= targcount; i++)
			{
				argtarr[i - 1] = ft->getParamType(i);
			}
			for (; i <= targcount + argcount; i++)
			{
				argtarr[i - 1] = ft->getParamType(i);
			}
			FunctionType* nft = FunctionType::get(ft->getReturnType(), ArrayRef<llvm::Type*>(argtarr, targcount + argcount), false);

			Function* cfun = Function::Create(nft, llvm::GlobalValue::ExternalLinkage, std::string("RT_NOM_CCC_").append(*cnstrins->GetSymbolName()), mod);

			BasicBlock* cblock = BasicBlock::Create(LLVMCONTEXT, "start", cfun);
			NomBuilder builder;

			builder->SetInsertPoint(cblock);

			Function* allocfun = GetNewAlloc(&mod);
			auto fieldCount = cls->GetFieldCount();
			llvm::Value* newmem = builder->CreateCall(allocfun, { MakeInt<size_t>(fieldCount + ((cls->GetHasRawInvoke() && NomLambdaOptimizationLevel > 0) ? 1 : 0)), MakeInt<size_t>(cls->GetTypeArgOffset() + cls->GetTypeParametersCount()) });

			newmem = ObjectHeader::GenerateSetClassDescriptor(builder, newmem, fieldCount, cls->GetLLVMElement(mod));
			if (cls->GetHasRawInvoke() && NomLambdaOptimizationLevel > 0)
			{
				RefValueHeader::GenerateWriteRawInvoke(builder, newmem, cls->GetRawInvokeFunction(mod));
			}

			auto argiter = cfun->arg_begin();
			llvm::Value** argarr = makealloca(llvm::Value*, targcount + argcount + 1);
			argarr[0] = newmem;
			for (i = 1; i <= targcount; i++)
			{
				argarr[i] = argiter;
				argiter++;
			}
			for (; i <= targcount + argcount; i++)
			{
				argarr[i] = argiter;
				argiter++;
			}
			auto callinst = builder->CreateCall(cnstrins->GetLLVMElement(mod), ArrayRef(argarr, targcount + argcount + 1));
			callinst->setCallingConv(NOMCC);
			cfun->setCallingConv(llvm::CallingConv::C);
			builder->CreateRet(callinst);

			llvm::raw_os_ostream out(std::cout);
			if (verifyFunction(*cfun, &out))
			{
				std::cout << "Could not verify C-accessible instance!";
				cfun->print(out);
				out.flush();
				std::cout.flush();
				throw cnstrins->GetSymbolName();
			}
			return cfun;
		}

		void CallInstance::Compile(NomBuilder& builder, CompileEnv* env, int lineno)
		{
			NomSubstitutionContextMemberContext nscmc(env->Context);
			NomInstantiationRef<NomClass> cls = NomConstants::GetSuperClass(ClassSuperClass)->GetClassType(&nscmc);

			auto fieldCount = cls.Elem->GetFieldCount();

			Function* allocfun = GetNewAlloc(env->Module);
			llvm::Value* newmem = builder->CreateCall(allocfun, { MakeInt<size_t>(fieldCount + ((cls.Elem->GetHasRawInvoke() && NomLambdaOptimizationLevel > 0) ? 1 : 0)), MakeInt<size_t>(cls.Elem->GetTypeArgOffset() + cls.Elem->GetTypeParametersCount()) });

			newmem = ObjectHeader::GenerateSetClassDescriptor(builder, newmem, fieldCount, cls.Elem->GetLLVMElement(*(env->Module)));
			if (cls.Elem->GetHasRawInvoke() && NomLambdaOptimizationLevel > 0)
			{
				RefValueHeader::GenerateWriteRawInvoke(builder, newmem, cls.Elem->GetRawInvokeFunction(*(env->Module)));
			}

			NomValue* argbuf = (NomValue*)(nmalloc(sizeof(NomValue) * env->GetArgCount()));
			for (int i = 0; i < env->GetArgCount(); i++)
			{
				argbuf[i] = env->GetArgument(i);
			}
			RegisterValue(env, cls.Elem->GenerateInstanceCall(builder, env, cls.TypeArgs, newmem, llvm::ArrayRef<NomValue>(argbuf, env->GetArgCount())));
			env->ClearArguments();

		}
		void CallInstance::Print(bool resolve)
		{
			cout << "CCall #";
			NomConstants::PrintConstant(ClassSuperClass, resolve);
			cout << " -> #" << std::dec << WriteRegister;
			cout << "\n";
		}
		void CallInstance::FillConstantDependencies(NOM_CONSTANT_DEPENCENCY_CONTAINER& result)
		{
			result.push_back(ClassSuperClass);
			result.push_back(TypeArgs);
		}
	}
}