#include "ConstructStruct.h"
#include <iostream>
#include "../NomConstants.h"
#include "../NomRecord.h"
#include "../NomDynamicType.h"
#include "../TypeOperations.h"
#include "../CallingConvConf.h"
#include "../NomField.h"
#include "../NomType.h"

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{
		ConstructStructInstruction::ConstructStructInstruction(RegIndex reg, ConstantID structure, ConstantID typeArgs) : NomValueInstruction(reg, OpCode::ConstructStruct), StructureID(structure), TypeArgsID(typeArgs)
		{
		}
		void ConstructStructInstruction::Compile(NomBuilder& builder, CompileEnv* env, int lineno)
		{
			NomRecord* structure = NomConstants::GetRecord(StructureID)->GetRecord();
			auto targcount = structure->GetDirectTypeParametersCount();
			llvm::Value** argbuf = makealloca(llvm::Value*, env->GetArgCount()+targcount);
			Function* constructorFun = structure->GetLLVMElement(*env->Module);
			FunctionType* cft = constructorFun->getFunctionType();
			NomSubstitutionContextMemberContext nscmc(env->Context);
			auto argtypes = structure->GetArgumentTypes(&nscmc);
			auto typeArgs = NomConstants::GetTypeList(TypeArgsID)->GetTypeList(&nscmc);
			for (decltype(targcount) i = 0; i < targcount; i++)
			{
				auto tparam = typeArgs[i]->GetLLVMElement(*builder->GetInsertBlock()->getParent()->getParent());
				argbuf[i] = tparam;
			}
			for (int i = 0; i < env->GetArgCount(); i++)
			{
				NomValue nv = env->GetArgument(i);
				argbuf[i+ targcount] = EnsureType(builder, env, nv, argtypes[i], cft->getParamType(i+targcount));
			}
			auto callinst = builder->CreateCall(constructorFun, llvm::ArrayRef<llvm::Value*>(argbuf, env->GetArgCount()+targcount), "struct");
			callinst->setCallingConv(NOMCC);
			env->ClearArguments();
			RegisterValue(env, NomValue(callinst, &NomDynamicType::Instance(), true));
		}
		void ConstructStructInstruction::Print(bool resolve)
		{
			cout << "Record ";
			NomConstants::PrintConstant(StructureID, resolve);
			cout << " -> #" << std::dec << WriteRegister;
			cout << "\n";
		}
		void ConstructStructInstruction::FillConstantDependencies(NOM_CONSTANT_DEPENCENCY_CONTAINER& result)
		{
			result.push_back(StructureID);
		}
	}
}