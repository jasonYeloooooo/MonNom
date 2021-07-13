#include "CompileEnv.h"
#include "ObjectHeader.h"
#include "NomMemberContext.h"
#include "NomType.h"
#include "llvm/ADT/ArrayRef.h"
#include "NomClassType.h"
#include "NomMemberContext.h"
#include "ObjectHeader.h"
#include "CompileHelpers.h"
#include "TypeOperations.h"
#include "NomDynamicType.h"
#include "NomStructType.h"
#include "NomMethod.h"
#include "NomStaticMethod.h"
#include "NomLambda.h"
#include "TypeOperations.h"
#include "NomInterface.h"
#include "LambdaHeader.h"
#include "NomConstructor.h"
#include "NomStruct.h"
#include "NomStructMethod.h"
#include "StructHeader.h"
#include "NomStructType.h"
#include "NomTypeParameter.h"

namespace Nom
{
	namespace Runtime
	{

		CompileEnv::CompileEnv(const llvm::Twine contextName, llvm::Function* function, const NomMemberContext* context) : contextName(contextName), Module(function==nullptr?nullptr:function->getParent()), Function(function), Context(context) {}
#pragma region ACompileEnv
		ACompileEnv::ACompileEnv(const RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, const std::vector<PhiNode*>* phiNodes, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const NomMemberContext* context, const TypeList argtypes, NomTypeRef thisType) : CompileEnv(contextName, function, context), regcount(regcount), registers(new NomValue[regcount]), phiNodes(phiNodes)
		{
			size_t argcount = 0;
			RegIndex argindex = 0;
			size_t typeArgCount = directTypeArgs.size();
			for (auto& Arg : function->args())
			{
				if (argcount==0&&thisType != nullptr)
				{
					registers[argindex] = NomValue(&Arg, thisType);
					argindex++;
				}
				else if (argcount + (thisType != nullptr ? 0 : 1) <= typeArgCount )
				{
					TypeArguments.push_back(NomTypeVarValue(&Arg, directTypeArgs[argcount- (thisType != nullptr ? 1 : 0)]->GetVariable()));
				}
				else
				{
					registers[argindex] = NomValue(&Arg, argtypes[argindex - (thisType == nullptr ? 0 : 1)]);
					argindex++;
				}
				argcount++;
			}
			if (argcount != typeArgCount + argtypes.size() + (thisType == nullptr ? 0 : 1))
			{
				throw new std::exception();
			}
		}

		ACompileEnv::~ACompileEnv()
		{
			delete[] registers;
		}



		NomValue ACompileEnv::GetArgument(int i)
		{
			return Arguments[i];
		}

		void ACompileEnv::PushArgument(NomValue arg)
		{
			Arguments.push_back(arg);
		}

		void ACompileEnv::ClearArguments()
		{
			Arguments.clear();
		}

		int ACompileEnv::GetArgCount()
		{
			return Arguments.size();
		}

		PhiNode* ACompileEnv::GetPhiNode(int index)
		{
			return (*phiNodes)[index];
		}

		size_t ACompileEnv::GetLocalTypeArgumentCount()
		{
			return TypeArguments.size();
		}

		llvm::Value* ACompileEnv::GetLocalTypeArgumentArray(NomBuilder& builder)
		{
			//if (localTypeArgArray == nullptr)
			//{
			auto argc = GetLocalTypeArgumentCount();
			localTypeArgArray = builder->CreateAlloca(TYPETYPE, MakeInt(argc));
			for (decltype(argc) i = 0; i < argc; i++)
			{
				MakeStore(builder, TypeArguments[i], builder->CreateGEP(localTypeArgArray, MakeInt32((int32_t)(argc - (i + 1)))));
			}
			//}
			return builder->CreateGEP(localTypeArgArray, MakeInt32(argc));
		}

#pragma endregion
#pragma region InstanceMethodCompileEnv
		InstanceMethodCompileEnv::InstanceMethodCompileEnv(RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, const std::vector<PhiNode*>* phiNodes, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, NomClassTypeRef thisType, const NomMethod* method) : ACompileEnv(regcount, contextName, function, phiNodes, directTypeArgs, method, argtypes, thisType), Method(method)
		{
		}

		NomTypeVarValue InstanceMethodCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			if ((size_t)i < Method->GetTypeParametersStart())
			{
				return NomTypeVarValue(ObjectHeader::GenerateReadTypeArgument(builder, registers[0], i + Method->GetContainer()->GetTypeParametersStart()), Context->GetTypeParameter(i)->GetVariable()); //needs to add container argument start because superclass variable instantiations may be different (e.g. B<T> extends A<Foo<T>>)
			}
			else
			{
				return TypeArguments[i - Method->GetTypeParametersStart()];
			}
		}
		size_t InstanceMethodCompileEnv::GetEnvTypeArgumentCount()
		{
			return Method->GetContainer()->GetTypeParametersCount();
		}
		llvm::Value* InstanceMethodCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return builder->CreateGEP(ObjectHeader::GeneratePointerToTypeArguments(builder, registers[0]), MakeInt32(-(uint32_t)Method->GetContainer()->GetTypeParametersStart()));
		}
#pragma endregion

#pragma region StaticMethodCompileEnv
		StaticMethodCompileEnv::StaticMethodCompileEnv(RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, const std::vector<PhiNode*>* phiNodes, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, const NomStaticMethod* method) : ACompileEnv(regcount, contextName, function, phiNodes, directTypeArgs, method, argtypes, nullptr), Method(method)
		{
		}

		NomTypeVarValue StaticMethodCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			return TypeArguments[i];
		}
		size_t StaticMethodCompileEnv::GetEnvTypeArgumentCount()
		{
			return 0;
		}
		llvm::Value* StaticMethodCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return ConstantPointerNull::get(TYPETYPE->getPointerTo());
		}
#pragma endregion

#pragma region ConstructorCompileEnv
		ConstructorCompileEnv::ConstructorCompileEnv(RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, const std::vector<PhiNode*>* phiNodes, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, NomClassTypeRef thisType, const NomConstructor* method) : ACompileEnv(regcount, contextName, function, phiNodes, directTypeArgs, method, argtypes, thisType), Method(method)
		{
		}

		NomTypeVarValue ConstructorCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			return TypeArguments[i];
		}
		size_t ConstructorCompileEnv::GetEnvTypeArgumentCount()
		{
			return 0;
		}
		llvm::Value* ConstructorCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return ConstantPointerNull::get(TYPETYPE->getPointerTo());
		}
#pragma endregion

#pragma region LambdaCompileEnv

		LambdaCompileEnv::LambdaCompileEnv(RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, const std::vector<PhiNode*>* phiNodes, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, const NomLambdaBody* lambda) : ACompileEnv(regcount, contextName, function, phiNodes, directTypeArgs, lambda, argtypes, &NomStructType::Instance()), Lambda(lambda)
		{
		}


		NomTypeVarValue LambdaCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			if ((size_t)i < Lambda->GetParent()->GetTypeParametersCount())
			{
				return NomTypeVarValue(LambdaHeader::GenerateReadTypeArgument(builder, registers[0], i, this->Lambda->Parent), Context->GetTypeParameter(i)->GetVariable());
			}
			else
			{
				return TypeArguments[i - Lambda->GetParent()->GetTypeParametersCount()];
			}
		}
		size_t LambdaCompileEnv::GetEnvTypeArgumentCount()
		{
			return Lambda->GetParent()->GetTypeParametersCount();
		}
		llvm::Value* LambdaCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return LambdaHeader::GeneratePointerToTypeArguments(builder, registers[0], this->Lambda->Parent);
		}

#pragma endregion
#pragma region NomStruct
		StructMethodCompileEnv::StructMethodCompileEnv(RegIndex regcount, const llvm::Twine contextName, llvm::Function* function, const std::vector<PhiNode*>* phiNodes, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, const NomStructMethod* method) : ACompileEnv(regcount, contextName, function, phiNodes, directTypeArgs, method, argtypes, &NomStructType::Instance()), Method(method)
		{
		}
		NomTypeVarValue StructMethodCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			if ((size_t)i < Method->Container->GetTypeParametersCount())
			{
				return NomTypeVarValue(StructHeader::GenerateReadTypeArgument(builder, registers[0], i, this->Method->Container), Context->GetTypeParameter(i)->GetVariable());
			}
			else
			{
				return TypeArguments[i - Method->Container->GetTypeParametersCount()];
			}
		}
		size_t StructMethodCompileEnv::GetEnvTypeArgumentCount()
		{
			return Method->Container->GetTypeParametersCount();
		}
		llvm::Value* StructMethodCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return StructHeader::GeneratePointerToTypeArguments(builder, registers[0], this->Method->Container);
		}
#pragma endregion
#pragma region NomStructInstantiation
		StructInstantiationCompileEnv::StructInstantiationCompileEnv(RegIndex regcount, llvm::Function* function, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, const NomStruct* structure, RegIndex endargregcount) : ACompileEnv(regcount, *structure->GetSymbolName(), function, nullptr, directTypeArgs, structure, argtypes, nullptr), Struct(structure)
		{
			int endArgBarrier = argtypes.size() - endargregcount;
			for (int i = argtypes.size(); i > 0; i--)
			{
				if (i > endArgBarrier)
				{
					(*this)[regcount - (i - argtypes.size() - 1)] = (*this)[i - 1];
				}
				else
				{
					(*this)[i] = (*this)[i - 1];
				}
			}
		}

		NomTypeVarValue StructInstantiationCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			if ((size_t)i < Context->GetTypeParametersCount())
			{
				return NomTypeVarValue(ObjectHeader::GenerateReadTypeArgument(builder, registers[0], i + Context->GetTypeParametersStart()), Context->GetTypeParameter(i)->GetVariable());
			}
			else
			{
				return TypeArguments[i - Context->GetTypeParametersStart()];
			}
		}
		size_t StructInstantiationCompileEnv::GetEnvTypeArgumentCount()
		{
			return Context->GetTypeParametersCount();
		}
		llvm::Value* StructInstantiationCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return builder->CreateGEP(ObjectHeader::GeneratePointerToTypeArguments(builder, registers[0]), MakeInt32(-(uint32_t)Context->GetTypeParametersStart()));
		}
#pragma endregion
#pragma region SimpleClassCompileEnv
		SimpleClassCompileEnv::SimpleClassCompileEnv(llvm::Function* function, const NomMemberContext* context, const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const TypeList argtypes, NomTypeRef thisType) : ACompileEnv(function->getFunctionType()->getNumParams() - directTypeArgs.size(), *context->GetSymbolName(), function, nullptr, directTypeArgs, context, argtypes, thisType)
		{

		}

		NomTypeVarValue SimpleClassCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			if ((size_t)i < Context->GetTypeParametersCount())
			{
				return NomTypeVarValue(ObjectHeader::GenerateReadTypeArgument(builder, registers[0], i + Context->GetTypeParametersStart()), Context->GetTypeParameter(i)->GetVariable());
			}
			else
			{
				return TypeArguments[i - Context->GetTypeParametersStart()];
			}
		}
		size_t SimpleClassCompileEnv::GetEnvTypeArgumentCount()
		{
			return Context->GetTypeParametersCount();
		}
		llvm::Value* SimpleClassCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return builder->CreateGEP(ObjectHeader::GeneratePointerToTypeArguments(builder, registers[0]), MakeInt32(-(uint32_t)Context->GetTypeParametersStart()));
		}
#pragma endregion
#pragma region CastedValueCompileEnv
		CastedValueCompileEnv::CastedValueCompileEnv(const llvm::ArrayRef<NomTypeParameterRef> directTypeArgs, const llvm::ArrayRef<NomTypeParameterRef> instanceTypeArgs, llvm::Value* localTypeArr, llvm::Value* instanceTypeArr)
			: CompileEnv("", nullptr, nullptr), directTypeArgs(directTypeArgs), instanceTypeArgs(instanceTypeArgs), localTypeArr(localTypeArr), instanceTypeArr(instanceTypeArr)
		{
		}
		NomValue& CastedValueCompileEnv::operator[](const RegIndex index)
		{
			throw new std::exception();
		}
		NomTypeVarValue CastedValueCompileEnv::GetTypeArgument(NomBuilder& builder, int i)
		{
			if (i > instanceTypeArgs.size())
			{
				return NomTypeVarValue(MakeLoad(builder, builder->CreateGEP(localTypeArr, MakeInt32(-((i - instanceTypeArgs.size()) + 1)))), new NomTypeVar(directTypeArgs[i-instanceTypeArgs.size()]));
			}
			else
			{
				return NomTypeVarValue(MakeLoad(builder, builder->CreateGEP(instanceTypeArr, MakeInt32(-(i+1)))), new NomTypeVar(instanceTypeArgs[i]));
			}
		}
		NomValue CastedValueCompileEnv::GetArgument(int i)
		{
			throw new std::exception();
		}
		void CastedValueCompileEnv::PushArgument(NomValue arg)
		{
			throw new std::exception();
		}
		void CastedValueCompileEnv::ClearArguments()
		{
			throw new std::exception();
		}
		int CastedValueCompileEnv::GetArgCount()
		{
			throw new std::exception();
		}
		PhiNode* CastedValueCompileEnv::GetPhiNode(int index)
		{
			throw new std::exception();
		}
		size_t CastedValueCompileEnv::GetLocalTypeArgumentCount()
		{
			return directTypeArgs.size();
		}
		size_t CastedValueCompileEnv::GetEnvTypeArgumentCount()
		{
			return instanceTypeArgs.size();
		}
		llvm::Value* CastedValueCompileEnv::GetLocalTypeArgumentArray(NomBuilder& builder)
		{
			return localTypeArr;
		}
		llvm::Value* CastedValueCompileEnv::GetEnvTypeArgumentArray(NomBuilder& builder)
		{
			return instanceTypeArr;
		}
#pragma endregion
}
}
