#include "CallDispatchBestMethod.h"
#include "../NomConstants.h"
#include "../NomNameRepository.h"
#include "../RTDictionary.h"
#include "../CompileHelpers.h"
#include "../RTOutput.h"
#include "../NomPartialApplication.h"
#include "../RTVTable.h"
#include "../RefValueHeader.h"
#include "../ObjectHeader.h"
#include "../RTClass.h"
#include "../LambdaHeader.h"
#include "../RTLambda.h"
#include "../NomDynamicType.h"
#include <iostream>
#include "../IntClass.h"
#include "../FloatClass.h"
#include "../RTStruct.h"
#include "../StructHeader.h"
#include "../CallingConvConf.h"
#include "../BoolClass.h"

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{
		CallDispatchBestMethod::CallDispatchBestMethod(RegIndex reg, RegIndex receiver, ConstantID methodName, ConstantID typeArgs) : NomValueInstruction(reg, OpCode::CallDispatchBest), Receiver(receiver), MethodName(methodName), TypeArguments(typeArgs)
		{
		}


		CallDispatchBestMethod::~CallDispatchBestMethod()
		{
		}

		llvm::Value* CallDispatchBestMethod::GenerateGetBestInvokeDispatcherDyn(NomBuilder& builder, NomValue receiver, llvm::Value* typeargcount, llvm::Value* argcount)
		{
			BasicBlock* origBlock = builder->GetInsertBlock();
			Function* fun = origBlock->getParent();

			BasicBlock* refValueBlock = nullptr, * packedIntBlock = nullptr, * packedFloatBlock = nullptr, * primitiveIntBlock = nullptr, * primitiveFloatBlock = nullptr, * primitiveBoolBlock = nullptr;

			int mergeBlocks = RefValueHeader::GenerateRefOrPrimitiveValueSwitch(builder, receiver, &refValueBlock, &packedIntBlock, &packedFloatBlock, false, &primitiveIntBlock, nullptr, &primitiveFloatBlock, nullptr, &primitiveBoolBlock, nullptr);

			BasicBlock* mergeBlock = BasicBlock::Create(LLVMCONTEXT, "ddLookupMerge", fun);
			PHINode* mergePHI = nullptr;
			Value* returnVal = nullptr;

			if (mergeBlocks == 0)
			{
				throw new std::exception();
			}
			if (mergeBlocks > 1)
			{
				builder->SetInsertPoint(mergeBlock);
				mergePHI = builder->CreatePHI(NomClass::GetDynamicDispatcherLookupResultType(), mergeBlocks);
				returnVal = mergePHI;
			}

			if (refValueBlock != nullptr)
			{
				BasicBlock* regularVTableBlock = nullptr, * lambdaBlock = nullptr, * structBlock = nullptr, * partialAppBlock = nullptr;
				builder->SetInsertPoint(refValueBlock);
				Value* vtable = nullptr;
				int mergeBlocks2 = RefValueHeader::GenerateVTableTagSwitch(builder, receiver, &vtable, &regularVTableBlock, &lambdaBlock, &structBlock, &partialAppBlock);

				BasicBlock* mergeBlock2 = BasicBlock::Create(LLVMCONTEXT, "ddLookupMerge2", fun);
				PHINode* mergePHI2 = nullptr;
				Value* returnVal2 = nullptr;
				builder->SetInsertPoint(mergeBlock2);
				if (mergeBlocks2 > 1)
				{
					mergePHI2 = builder->CreatePHI(NomClass::GetDynamicDispatcherLookupResultType(), mergeBlocks2);
					returnVal2 = mergePHI2;
				}
				builder->CreateBr(mergeBlock);

				if (regularVTableBlock != nullptr)
				{
					builder->SetInsertPoint(regularVTableBlock);
					BasicBlock* classObjectBlock = nullptr, * vtlambdaBlock = nullptr, * vtstructBlock = nullptr, * vtpartialAppBlock = nullptr, * multiCastBlock = nullptr;

					int mergeBlocks3 = RTVTable::GenerateVTableKindSwitch(builder, vtable, &classObjectBlock, &vtlambdaBlock, &vtstructBlock, &vtpartialAppBlock, &multiCastBlock);

					BasicBlock* mergeBlock3 = BasicBlock::Create(LLVMCONTEXT, "ddLookupMerge3", fun);
					PHINode* mergePHI3 = nullptr;
					Value* returnVal3 = nullptr;
					builder->SetInsertPoint(mergeBlock3);
					if (mergeBlocks3 > 1)
					{
						mergePHI3 = builder->CreatePHI(NomClass::GetDynamicDispatcherLookupResultType(), mergeBlocks3);
						returnVal3 = mergePHI3;
					}
					builder->CreateBr(mergeBlock2);

					if (classObjectBlock != nullptr)
					{
						builder->SetInsertPoint(classObjectBlock);
						auto dispatcherLookupPtr = RTClass::GenerateReadDispatcherLookup(builder, vtable);
						auto dispatcherLookupCall = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), dispatcherLookupPtr, { receiver, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID("")), typeargcount, argcount }, "dispatcher");
						dispatcherLookupCall->setCallingConv(NOMCC);
						if (mergePHI3 != nullptr)
						{
							mergePHI3->addIncoming(dispatcherLookupCall, builder->GetInsertBlock());
						}
						else
						{
							returnVal3 = dispatcherLookupCall;
						}
						builder->CreateBr(mergeBlock3);
					}

					if (vtlambdaBlock != nullptr)
					{
						BasicBlock* lambdaMatch = BasicBlock::Create(LLVMCONTEXT, "castedLambdaArityMatch", fun);
						BasicBlock* wrongArity = RTOutput_Fail::GenerateFailOutputBlock(builder, "Trying to call lambda with wrong arity!");
						builder->SetInsertPoint(vtlambdaBlock);
						auto lambdaMeta = LambdaHeader::GenerateReadLambdaMetadata(builder, receiver);
						auto lambdaArityMatch = RTLambda::GenerateCheckArgCountsMatch(builder, lambdaMeta, typeargcount, argcount);
						builder->CreateCondBr(lambdaArityMatch, lambdaMatch, wrongArity);

						builder->SetInsertPoint(lambdaMatch);
						auto ldispatcher = RTLambda::GenerateReadDispatcherPointer(builder, lambdaMeta);
						auto ldispatcherPair = builder->CreateInsertValue(UndefValue::get(NomClass::GetDynamicDispatcherLookupResultType()), builder->CreatePointerCast(ldispatcher, POINTERTYPE), { 0 });
						ldispatcherPair = builder->CreateInsertValue(ldispatcherPair, receiver, { 1 });

						if (mergePHI3 != nullptr)
						{
							mergePHI3->addIncoming(ldispatcherPair, builder->GetInsertBlock());
						}
						else
						{
							returnVal3 = ldispatcherPair;
						}
						builder->CreateBr(mergeBlock3);
					}

					if (vtstructBlock != nullptr)
					{
						RTOutput_Fail::MakeBlockFailOutputBlock(builder, "dynamic struct invoke not implemented", vtstructBlock);
					}

					if (vtpartialAppBlock != nullptr)
					{
						RTOutput_Fail::MakeBlockFailOutputBlock(builder, "dynamic partial app invoke not implemented", vtpartialAppBlock);
					}

					if (multiCastBlock != nullptr)
					{
						RTOutput_Fail::MakeBlockFailOutputBlock(builder, "dynamic invoke on multicasted values not implemented", multiCastBlock);
					}
					if (mergePHI2 != nullptr)
					{
						mergePHI2->addIncoming(returnVal3, mergeBlock3);
					}
					else
					{
						returnVal2 = returnVal3;
					}
				}


				if (lambdaBlock != nullptr)
				{
					BasicBlock* lambdaMatch = BasicBlock::Create(LLVMCONTEXT, "lambdaArityMatch", fun);
					BasicBlock* wrongArity = RTOutput_Fail::GenerateFailOutputBlock(builder, "Trying to call lambda with wrong arity!");
					builder->SetInsertPoint(lambdaBlock);
					auto lambdaMeta = LambdaHeader::GenerateReadLambdaMetadata(builder, receiver);
					auto lambdaArityMatch = RTLambda::GenerateCheckArgCountsMatch(builder, lambdaMeta, typeargcount, argcount);
					builder->CreateCondBr(lambdaArityMatch, lambdaMatch, wrongArity);

					builder->SetInsertPoint(lambdaMatch);
					auto ldispatcher = RTLambda::GenerateReadDispatcherPointer(builder, lambdaMeta);
					auto ldispatcherPair = builder->CreateInsertValue(UndefValue::get(NomClass::GetDynamicDispatcherLookupResultType()), builder->CreatePointerCast(ldispatcher, POINTERTYPE), { 0 });
					ldispatcherPair = builder->CreateInsertValue(ldispatcherPair, receiver, { 1 });

					if (mergePHI2 != nullptr)
					{
						mergePHI2->addIncoming(ldispatcherPair, builder->GetInsertBlock());
					}
					else
					{
						returnVal2 = ldispatcherPair;
					}
					builder->CreateBr(mergeBlock2);
				}

				if (structBlock != nullptr)
				{
					RTOutput_Fail::MakeBlockFailOutputBlock(builder, "dynamic partial app invoke not implemented", structBlock);
				}
				if (partialAppBlock != nullptr)
				{
					RTOutput_Fail::MakeBlockFailOutputBlock(builder, "dynamic partial app invoke not implemented", partialAppBlock);
				}

				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(returnVal2, mergeBlock2);
				}
				else
				{
					returnVal = returnVal2;
				}
			}

			if (packedIntBlock != nullptr)
			{
				RTOutput_Fail::MakeBlockFailOutputBlock(builder, "Cannot invoke integer values!", packedIntBlock);
			}
			if (packedFloatBlock != nullptr)
			{
				RTOutput_Fail::MakeBlockFailOutputBlock(builder, "Cannot invoke float values!", packedFloatBlock);
			}
			if (primitiveIntBlock != nullptr)
			{
				RTOutput_Fail::MakeBlockFailOutputBlock(builder, "Cannot invoke integer values!", primitiveIntBlock);
			}
			if (primitiveFloatBlock != nullptr)
			{
				RTOutput_Fail::MakeBlockFailOutputBlock(builder, "Cannot invoke float values!", primitiveFloatBlock);
			}
			if (primitiveBoolBlock != nullptr)
			{
				RTOutput_Fail::MakeBlockFailOutputBlock(builder, "Cannot invoke boolean values!", primitiveBoolBlock);
			}

			builder->SetInsertPoint(mergeBlock);
			return returnVal;
		}


		llvm::Value* CallDispatchBestMethod::GenerateBestInvoke(NomBuilder& builder, llvm::Value* receiver, uint32_t typeargcount, uint32_t argcount, llvm::ArrayRef<llvm::Value*> args)
		{
			auto dispatcherPHI = GenerateGetBestInvokeDispatcherDyn(builder, receiver, MakeInt32(typeargcount), MakeInt32(argcount));
			auto dispatcherPtr = builder->CreatePointerCast(builder->CreateExtractValue(dispatcherPHI, { 0 }), NomPartialApplication::GetDynamicDispatcherType(typeargcount, argcount)->getPointerTo());
			auto callinst = builder->CreateCall(NomPartialApplication::GetDynamicDispatcherType(typeargcount, argcount), dispatcherPtr, args, "[INVOKE]");
			callinst->setCallingConv(NOMCC);
			return callinst;
		}

		NomValue NVGenerateBestInvoke(NomBuilder& builder, CompileEnv* env, llvm::Value* receiver, TypeList typeargs/*, intptr_t** outerbuffer*/)
		{
			uint32_t typeargcount = typeargs.size();
			uint32_t argcount = typeargcount + env->GetArgCount() + 1;
			Value** argbuf = makealloca(Value*, argcount + 1);
			uint32_t argbufpos = 0;
			while (argbufpos < typeargcount)
			{
				argbuf[argbufpos] = typeargs[argbufpos]->GetLLVMElement(*env->Module);
				argbufpos++;
			}
			argbuf[argbufpos] = receiver;
			argbufpos++;
			while (argbufpos < argcount)
			{
				argbuf[argbufpos] = env->GetArgument(argbufpos - typeargcount - 1);
				argbufpos++;
			}
			return NomValue(CallDispatchBestMethod::GenerateBestInvoke(builder, receiver, typeargcount, env->GetArgCount(), llvm::ArrayRef<Value*>(argbuf, argbufpos)), &NomDynamicType::Instance());
		}


		void CallDispatchBestMethod::Compile(NomBuilder& builder, CompileEnv* env, int lineno)
		{
			NomValue receiver = (*env)[Receiver];
			BasicBlock* origBlock = builder->GetInsertBlock();
			Function* fun = origBlock->getParent();

			NomSubstitutionContextMemberContext nscmc(env->Context);
			auto typeargs = NomConstants::GetTypeList(TypeArguments)->GetTypeList(&nscmc);
			auto methodName = NomConstants::GetString(MethodName)->GetText()->ToStdString();

			if (methodName.empty())
			{
				RegisterValue(env, NVGenerateBestInvoke(builder, env, receiver, typeargs));
				env->ClearArguments();
				return;
			}

			int typeargcount = typeargs.size();
			int argcount = env->GetArgCount();

			BasicBlock* refValueBlock = nullptr, * packedIntBlock = nullptr, * packedFloatBlock = nullptr, * primitiveIntBlock = nullptr, * primitiveFloatBlock = nullptr, * primitiveBoolBlock = nullptr;
			Value* primitiveIntVal, *primitiveFloatVal, * primitiveBoolVal;

			int mergeBlocks = RefValueHeader::GenerateRefOrPrimitiveValueSwitch(builder, receiver, &refValueBlock, &packedIntBlock, &packedFloatBlock, false, &primitiveIntBlock, &primitiveIntVal, &primitiveFloatBlock, &primitiveFloatVal, &primitiveBoolBlock, &primitiveBoolVal);

			BasicBlock* mergeBlock = BasicBlock::Create(LLVMCONTEXT, "ddLookupMerge", fun);
			PHINode* mergePHI = nullptr;
			Value* returnVal = nullptr;

			if (mergeBlocks == 0)
			{
				throw new std::exception();
			}
			if (mergeBlocks > 1)
			{
				builder->SetInsertPoint(mergeBlock);
				mergePHI = builder->CreatePHI(NomClass::GetDynamicDispatcherLookupResultType(), mergeBlocks);
				returnVal = mergePHI;
			}

			if (refValueBlock != nullptr)
			{
				BasicBlock* regularVTableBlock = nullptr, * lambdaBlock = nullptr, * structBlock = nullptr, * partialAppBlock = nullptr;
				builder->SetInsertPoint(refValueBlock);
				Value* vtable = nullptr;
				int mergeBlocks2 = RefValueHeader::GenerateVTableTagSwitch(builder, receiver, &vtable, &regularVTableBlock, &lambdaBlock, &structBlock, &partialAppBlock);

				BasicBlock* mergeBlock2 = BasicBlock::Create(LLVMCONTEXT, "ddLookupMerge2", fun);
				PHINode* mergePHI2 = nullptr;
				Value* returnVal2 = nullptr;
				builder->SetInsertPoint(mergeBlock2);
				if (mergeBlocks2 > 1)
				{
					mergePHI2 = builder->CreatePHI(NomClass::GetDynamicDispatcherLookupResultType(), mergeBlocks2);
					returnVal2 = mergePHI2;
				}
				builder->CreateBr(mergeBlock);

				if (regularVTableBlock != nullptr)
				{
					builder->SetInsertPoint(regularVTableBlock);
					BasicBlock* classObjectBlock = nullptr, * vtlambdaBlock = nullptr, * vtstructBlock = nullptr, * vtpartialAppBlock = nullptr, * multiCastBlock = nullptr;

					int mergeBlocks3 = RTVTable::GenerateVTableKindSwitch(builder, vtable, &classObjectBlock, &vtlambdaBlock, &vtstructBlock, &vtpartialAppBlock, &multiCastBlock);

					BasicBlock* mergeBlock3 = BasicBlock::Create(LLVMCONTEXT, "ddLookupMerge3", fun);
					PHINode* mergePHI3 = nullptr;
					Value* returnVal3 = nullptr;
					builder->SetInsertPoint(mergeBlock3);
					if (mergeBlocks3 > 1)
					{
						mergePHI3 = builder->CreatePHI(NomClass::GetDynamicDispatcherLookupResultType(), mergeBlocks3);
						returnVal3 = mergePHI3;
					}
					builder->CreateBr(mergeBlock2);

					if (classObjectBlock != nullptr)
					{
						builder->SetInsertPoint(classObjectBlock);
						auto dispatcherLookupPtr = RTClass::GenerateReadDispatcherLookup(builder, vtable);
						auto dispatcherLookupCall = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), dispatcherLookupPtr, { receiver, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt32(typeargcount), MakeInt32(argcount) }, "dispatcher");
						dispatcherLookupCall->setCallingConv(NOMCC);
						if (mergePHI3 != nullptr)
						{
							mergePHI3->addIncoming(dispatcherLookupCall, builder->GetInsertBlock());
						}
						else
						{
							returnVal3 = dispatcherLookupCall;
						}
						builder->CreateBr(mergeBlock3);
					}

					if (vtlambdaBlock != nullptr)
					{
						RTOutput_Fail::MakeBlockFailOutputBlock(builder, "lambdas have no named members!", vtlambdaBlock);
					}

					if (vtstructBlock != nullptr)
					{
						builder->SetInsertPoint(vtstructBlock);
						auto structDesc = StructHeader::GenerateReadStructDescriptor(builder, receiver);
						auto structDispatcherLookupPtr = RTStruct::GenerateReadDispatcherLookup(builder, structDesc);
						auto structDispatcher = builder->CreateCall(NomStruct::GetDynamicDispatcherLookupType(), structDispatcherLookupPtr, { receiver, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(argcount) });
						if (mergePHI3 != nullptr)
						{
							mergePHI3->addIncoming(structDispatcher, builder->GetInsertBlock());
						}
						else
						{
							returnVal3 = structDispatcher;
						}
						builder->CreateBr(mergeBlock3);
					}

					if (vtpartialAppBlock != nullptr)
					{
						RTOutput_Fail::MakeBlockFailOutputBlock(builder, "partial applications have no named members!", vtpartialAppBlock);
					}

					if (multiCastBlock != nullptr)
					{
						RTOutput_Fail::MakeBlockFailOutputBlock(builder, "dynamic invoke on multicasted values not implemented", multiCastBlock);
					}

					if (mergePHI2 != nullptr)
					{
						mergePHI2->addIncoming(returnVal3, mergeBlock3);
					}
					else
					{
						returnVal2 = returnVal3;
					}
				}


				if (lambdaBlock != nullptr)
				{
					RTOutput_Fail::MakeBlockFailOutputBlock(builder, "lambdas have no named members!", lambdaBlock);
				}

				if (structBlock != nullptr)
				{
					builder->SetInsertPoint(structBlock);
					auto structDesc = StructHeader::GenerateReadStructDescriptor(builder, receiver);
					auto structDispatcherLookupPtr = RTStruct::GenerateReadDispatcherLookup(builder, structDesc);
					auto structDispatcher = builder->CreateCall(NomStruct::GetDynamicDispatcherLookupType(), structDispatcherLookupPtr, { receiver, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(argcount) });
					if (mergePHI2 != nullptr)
					{
						mergePHI2->addIncoming(structDispatcher, builder->GetInsertBlock());
					}
					else
					{
						returnVal2 = structDispatcher;
					}
					builder->CreateBr(mergeBlock2);
				}
				if (partialAppBlock != nullptr)
				{
					RTOutput_Fail::MakeBlockFailOutputBlock(builder, "partial applications have no named members", partialAppBlock);
				}

				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(returnVal2, mergeBlock2);
				}
				else
				{
					returnVal = returnVal2;
				}
			}

			if (packedIntBlock != nullptr)
			{
				builder->SetInsertPoint(packedIntBlock);
				auto intDispatcher = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), RTClass::GenerateReadDispatcherLookup(builder, NomIntClass::GetInstance()->GetLLVMElement(*env->Module)), { receiver, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(env->GetArgCount()) });
				intDispatcher->setCallingConv(NOMCC);
				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(intDispatcher, builder->GetInsertBlock());
				}
				else
				{
					returnVal = intDispatcher;
				}
				builder->CreateBr(mergeBlock);

			}
			if (packedFloatBlock != nullptr)
			{
				builder->SetInsertPoint(packedFloatBlock);
				auto floatDispatcher = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), RTClass::GenerateReadDispatcherLookup(builder, NomFloatClass::GetInstance()->GetLLVMElement(*env->Module)), { receiver, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(env->GetArgCount()) });
				floatDispatcher->setCallingConv(NOMCC);
				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(floatDispatcher, builder->GetInsertBlock());
				}
				else
				{
					returnVal = floatDispatcher;
				}
				builder->CreateBr(mergeBlock);
			}
			if (primitiveIntBlock != nullptr)
			{
				builder->SetInsertPoint(primitiveIntBlock);
				auto packedInt = PackInt(builder, receiver);
				auto intDispatcher = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), RTClass::GenerateReadDispatcherLookup(builder, NomIntClass::GetInstance()->GetLLVMElement(*env->Module)), { packedInt, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(env->GetArgCount()) });
				intDispatcher->setCallingConv(NOMCC);
				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(intDispatcher, builder->GetInsertBlock());
				}
				else
				{
					returnVal = intDispatcher;
				}
				builder->CreateBr(mergeBlock);
			}
			if (primitiveFloatBlock != nullptr)
			{
				builder->SetInsertPoint(primitiveFloatBlock);
				auto packedFloat = PackFloat(builder, receiver);
				auto floatDispatcher = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), RTClass::GenerateReadDispatcherLookup(builder, NomFloatClass::GetInstance()->GetLLVMElement(*env->Module)), { packedFloat, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(env->GetArgCount()) });
				floatDispatcher->setCallingConv(NOMCC);
				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(floatDispatcher, builder->GetInsertBlock());
				}
				else
				{
					returnVal = floatDispatcher;
				}
				builder->CreateBr(mergeBlock);
			}
			if (primitiveBoolBlock != nullptr)
			{
				builder->SetInsertPoint(primitiveBoolBlock);
				auto packedBool = PackBool(builder, receiver);
				auto boolDispatcher = builder->CreateCall(NomClass::GetDynamicDispatcherLookupType(), RTClass::GenerateReadDispatcherLookup(builder, NomBoolClass::GetInstance()->GetLLVMElement(*env->Module)), { packedBool, MakeInt<DICTKEYTYPE>(NomNameRepository::Instance().GetNameID(methodName)), MakeInt<int32_t>(typeargcount), MakeInt<int32_t>(env->GetArgCount()) });
				boolDispatcher->setCallingConv(NOMCC);
				if (mergePHI != nullptr)
				{
					mergePHI->addIncoming(boolDispatcher, builder->GetInsertBlock());
				}
				else
				{
					returnVal = boolDispatcher;
				}
				builder->CreateBr(mergeBlock);
			}

			builder->SetInsertPoint(mergeBlock);

			size_t dispargcount = typeargcount + argcount + 1;
			Value** argbuf = makealloca(Value*, dispargcount);
			size_t argbufpos = 0;
			while (argbufpos < typeargcount)
			{
				argbuf[argbufpos] = typeargs[argbufpos]->GetLLVMElement(*(env->Module));
				argbufpos++;
			}
			argbuf[argbufpos] = builder->CreateExtractValue(returnVal, { 1 });
			argbufpos++;
			while (argbufpos < dispargcount)
			{
				argbuf[argbufpos] = EnsurePacked(builder, env->GetArgument(argbufpos - typeargcount - 1));
				argbufpos++;
			}

			auto dispatcherCallInst = builder->CreateCall(NomPartialApplication::GetDynamicDispatcherType(typeargcount, env->GetArgCount()), builder->CreatePointerCast(builder->CreateExtractValue(returnVal, { 0 }), NomPartialApplication::GetDynamicDispatcherType(typeargcount, env->GetArgCount())->getPointerTo()), llvm::ArrayRef<Value*>(argbuf, dispargcount), methodName + "()");
			dispatcherCallInst->setCallingConv(NOMCC);
			env->ClearArguments();
			RegisterValue(env, NomValue(dispatcherCallInst, &NomDynamicType::Instance(), true));
		}



		void CallDispatchBestMethod::Print(bool resolve)
		{
			cout << "Dispatch #" << std::dec << Receiver;
			cout << " ";
			NomConstants::PrintConstant(MethodName, resolve);
			cout << "<";
			NomConstants::PrintConstant(TypeArguments, resolve);
			cout << "> -> #" << std::dec << WriteRegister;
			cout << "\n";
		}

		void CallDispatchBestMethod::FillConstantDependencies(NOM_CONSTANT_DEPENCENCY_CONTAINER& result)
		{
			result.push_back(MethodName);
			result.push_back(TypeArguments);
		}

	}
}
