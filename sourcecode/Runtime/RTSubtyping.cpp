#include "RTSubtyping.h"
#include "Defs.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/BasicBlock.h"
#include "RTTypeHead.h"
#include "RTClassType.h"
#include "RTTypeVar.h"
#include "NomClassType.h"
#include "RTInstanceType.h"
#include "RTDisjointness.h"
#include "NomInterface.h"
#include <iostream>
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/IR/Verifier.h"
#include "CompileHelpers.h"
#include "RTConfig.h"
#include "CastStats.h"
#include "RTInterface.h"
#include "CallingConvConf.h"
#include "RTCast.h"
#include "NullClass.h"
#include "NomMaybeType.h"
#include "NomTypeVar.h"
#include "RTTypeEq.h"
#include "RTMaybeType.h"
#include "Metadata.h"

using namespace llvm;
namespace Nom
{
	namespace Runtime
	{

		RTSubtyping& RTSubtyping::Instance()
		{
			static RTSubtyping rts; return rts;
		}

		RTSubtyping::RTSubtyping()
		{

		}

		RTSubtyping::~RTSubtyping()
		{
		}


		llvm::Function* RTSubtyping::createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const
		{
			llvm::FunctionType* funType = SubtypingFunctionType();
			llvm::Function* fun = llvm::Function::Create(funType, linkage, "NOM_RT_Subtyping", &mod);
			fun->setCallingConv(NOMCC);
			llvm::Argument* left = fun->arg_begin();
			llvm::Argument* right = left + 1;
			llvm::Argument* leftsubst = left + 2;
			llvm::Argument* rightsubst = left + 3;
			NomBuilder builder;
			BasicBlock* startBlock = BasicBlock::Create(LLVMCONTEXT, "start", fun);
			BasicBlock* leftUnfoldBlock = BasicBlock::Create(LLVMCONTEXT, "leftVariableUnfold", fun);
			BasicBlock* successBlock = BasicBlock::Create(LLVMCONTEXT, "subtypingSuccess", fun);
			BasicBlock* optimisticBlock = BasicBlock::Create(LLVMCONTEXT, "subtypingOptimisticSuccess", fun);
			BasicBlock* failBlock = BasicBlock::Create(LLVMCONTEXT, "subtypingFail", fun);
			builder->SetInsertPoint(startBlock);
			if (NomCastStats)
			{
				builder->CreateCall(GetIncSubtypingChecksFunction(mod), {});
			}
			auto argsEqual = builder->CreateICmpEQ(builder->CreatePtrToInt(left, numtype(intptr_t)), builder->CreatePtrToInt(right, numtype(intptr_t)));
			argsEqual = builder->CreateAnd({ argsEqual, builder->CreateIsNull(leftsubst), builder->CreateIsNull(rightsubst) });

			builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { argsEqual, MakeUInt(1,0) });
			builder->CreateCondBr(argsEqual, successBlock, leftUnfoldBlock, GetLikelySecondBranchMetadata());

			builder->SetInsertPoint(leftUnfoldBlock);
			PHINode* leftPHI = builder->CreatePHI(left->getType(), 2);
			PHINode* leftSubstPHI = builder->CreatePHI(leftsubst->getType(), 2);
			leftPHI->addIncoming(left, startBlock);
			leftSubstPHI->addIncoming(leftsubst, startBlock);


			BasicBlock* leftClassTypeBlock = nullptr, * leftTypeVarBlock = nullptr, * leftTopTypeBlock = nullptr, * leftInstanceTypeBlock = nullptr, * leftMaybeTypeBlock = nullptr, * leftDynamicTypeBlock = nullptr;
			RTTypeHead::GenerateTypeKindSwitch(builder, leftPHI, &leftClassTypeBlock, &leftTopTypeBlock, &leftTypeVarBlock, &successBlock, &leftInstanceTypeBlock, nullptr, nullptr, &leftDynamicTypeBlock, &leftMaybeTypeBlock);

			if (leftTypeVarBlock != nullptr)
			{
				builder->SetInsertPoint(leftTypeVarBlock);
				auto leftTypeArr = MakeLoad(builder, leftSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Types));
				auto newLeft = MakeLoad(builder, builder->CreateGEP(leftTypeArr, builder->CreateSub(MakeInt32(-1), RTTypeVar::GenerateLoadIndex(builder, leftPHI))));
				auto newsubst = MakeLoad(builder, leftSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Next));
				leftPHI->addIncoming(newLeft, builder->GetInsertBlock());
				leftSubstPHI->addIncoming(newsubst, builder->GetInsertBlock());
				builder->CreateBr(leftUnfoldBlock);
			}

			if (leftTopTypeBlock != nullptr)
			{
				BasicBlock* rightTypeVarBlock = nullptr;
				builder->SetInsertPoint(leftTopTypeBlock);
				auto rightPHI = builder->CreatePHI(right->getType(), 2);
				auto rightSubstPHI = builder->CreatePHI(rightsubst->getType(), 2);
				rightPHI->addIncoming(right, leftUnfoldBlock);
				rightSubstPHI->addIncoming(rightsubst, leftUnfoldBlock);
				RTTypeHead::GenerateTypeKindSwitch(builder, rightPHI, nullptr, &successBlock, &rightTypeVarBlock, nullptr, nullptr, nullptr, nullptr, &successBlock, nullptr, failBlock);

				if (rightTypeVarBlock != nullptr)
				{
					builder->SetInsertPoint(rightTypeVarBlock);
					auto rightTypeArr = MakeLoad(builder, rightSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Types));
					auto newRight = MakeLoad(builder, builder->CreateGEP(rightTypeArr, builder->CreateSub(MakeInt32(-1), RTTypeVar::GenerateLoadIndex(builder, rightPHI))));
					auto newsubst = MakeLoad(builder, rightSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Next));
					rightPHI->addIncoming(newRight, builder->GetInsertBlock());
					rightSubstPHI->addIncoming(newsubst, builder->GetInsertBlock());
					builder->CreateBr(leftTopTypeBlock);
				}
			}

			if (leftMaybeTypeBlock != nullptr)
			{
				BasicBlock* superOfNullBlock = BasicBlock::Create(LLVMCONTEXT, "rightSuperOfNull", fun);
				builder->SetInsertPoint(leftMaybeTypeBlock);
				auto superOfNull = builder->CreateCall(fun, { NomNullClass::GetInstance()->GetType()->GetLLVMElement(mod), right, ConstantPointerNull::get(TypeArgumentListStackType()->getPointerTo()), rightsubst });
				superOfNull->setCallingConv(NOMCC);
				auto success = builder->CreateICmpEQ(superOfNull, MakeUInt(2, 3));
				builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { success, MakeUInt(1,1) });
				builder->CreateCondBr(success, superOfNullBlock, failBlock, GetLikelyFirstBranchMetadata());

				builder->SetInsertPoint(superOfNullBlock);
				auto superOfOther = builder->CreateCall(fun, { RTMaybeType::GenerateReadPotentialType(builder, leftPHI), right, leftSubstPHI, rightsubst });
				superOfOther->setCallingConv(NOMCC);
				success = builder->CreateICmpEQ(superOfOther, MakeUInt(2, 3));
				builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { success, MakeUInt(1,1) });
				builder->CreateCondBr(success, successBlock, failBlock, GetLikelyFirstBranchMetadata());
			}

			if (leftDynamicTypeBlock != nullptr)
			{
				builder->SetInsertPoint(leftDynamicTypeBlock);
				BasicBlock* rightTypeVarBlock = nullptr;
				auto rightPHI = builder->CreatePHI(right->getType(), 2);
				auto rightSubstPHI = builder->CreatePHI(rightsubst->getType(), 2);
				rightPHI->addIncoming(right, leftUnfoldBlock);
				rightSubstPHI->addIncoming(rightsubst, leftUnfoldBlock);
				RTTypeHead::GenerateTypeKindSwitch(builder, rightPHI, &optimisticBlock, &successBlock, &rightTypeVarBlock, &optimisticBlock, &optimisticBlock, nullptr, nullptr, &successBlock, &optimisticBlock, failBlock);

				if (rightTypeVarBlock != nullptr)
				{
					builder->SetInsertPoint(rightTypeVarBlock);
					auto rightTypeArr = MakeLoad(builder, rightSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Types));
					auto newRight = MakeLoad(builder, builder->CreateGEP(rightTypeArr, builder->CreateSub(MakeInt32(-1), RTTypeVar::GenerateLoadIndex(builder, rightPHI))));
					auto newsubst = MakeLoad(builder, rightSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Next));
					rightPHI->addIncoming(newRight, builder->GetInsertBlock());
					rightSubstPHI->addIncoming(newsubst, builder->GetInsertBlock());
					builder->CreateBr(leftDynamicTypeBlock);
				}
			}

			//now on to the two versions of class types
			{
				BasicBlock* leftClsMergeBlock = BasicBlock::Create(LLVMCONTEXT, "leftClassInfoMerge", fun);
				builder->SetInsertPoint(leftClsMergeBlock);
				auto leftTypeArgsPHI = builder->CreatePHI(TYPETYPE->getPointerTo(), 2, "leftTypeArgs");
				auto leftIfacePHI = builder->CreatePHI(RTInterface::GetLLVMType()->getPointerTo(), 2, "leftInterfacePtr");

				if (leftClassTypeBlock != nullptr)
				{
					builder->SetInsertPoint(leftClassTypeBlock);
					leftTypeArgsPHI->addIncoming(RTClassType::GetTypeArgumentsPtr(builder, leftPHI), builder->GetInsertBlock());
					leftIfacePHI->addIncoming(RTClassType::GenerateReadClassDescriptorLink(builder, leftPHI), builder->GetInsertBlock());
					builder->CreateBr(leftClsMergeBlock);

				}
				if (leftInstanceTypeBlock != nullptr)
				{
					builder->SetInsertPoint(leftInstanceTypeBlock);
					leftTypeArgsPHI->addIncoming(RTInstanceType::GetTypeArgumentsPtr(builder, leftPHI), builder->GetInsertBlock());
					leftIfacePHI->addIncoming(RTInstanceType::GenerateReadClassDescriptorLink(builder, leftPHI), builder->GetInsertBlock());
					builder->CreateBr(leftClsMergeBlock);
				}

				builder->SetInsertPoint(leftClsMergeBlock);
				BasicBlock* rightMerge = BasicBlock::Create(LLVMCONTEXT, "rightVarUnfold", fun);
				builder->CreateBr(rightMerge);

				builder->SetInsertPoint(rightMerge);
				auto rightPHI = builder->CreatePHI(right->getType(), 2);
				auto rightSubstPHI = builder->CreatePHI(rightsubst->getType(), 2);
				rightPHI->addIncoming(right, leftClsMergeBlock);
				rightSubstPHI->addIncoming(rightsubst, leftClsMergeBlock);

				BasicBlock* rightTypeVarBlock = nullptr, * rightClassTypeBlock = nullptr, * rightInstanceTypeBlock = nullptr, * rightMaybeTypeBlock = nullptr;
				RTTypeHead::GenerateTypeKindSwitch(builder, rightPHI, &rightClassTypeBlock, &successBlock, &rightTypeVarBlock, &failBlock, &rightInstanceTypeBlock, nullptr, nullptr, &successBlock, &rightMaybeTypeBlock, failBlock);

				if (rightTypeVarBlock != nullptr)
				{
					builder->SetInsertPoint(rightTypeVarBlock);
					auto rightTypeArr = MakeLoad(builder, rightSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Types));
					auto newRight = MakeLoad(builder, builder->CreateGEP(rightTypeArr, builder->CreateSub(MakeInt32(-1), RTTypeVar::GenerateLoadIndex(builder, rightPHI))));
					auto newsubst = MakeLoad(builder, rightSubstPHI, MakeInt<TypeArgumentListStackFields>(TypeArgumentListStackFields::Next));
					rightPHI->addIncoming(newRight, builder->GetInsertBlock());
					rightSubstPHI->addIncoming(newsubst, builder->GetInsertBlock());
					builder->CreateBr(rightMerge);
				}

				if (rightMaybeTypeBlock != nullptr)
				{
					BasicBlock* notSubOfNullBlock = BasicBlock::Create(LLVMCONTEXT, "notSubOfNull", fun);
					builder->SetInsertPoint(rightMaybeTypeBlock);
					auto subOfNull = builder->CreateCall(fun, { leftPHI, NomNullClass::GetInstance()->GetType()->GetLLVMElement(mod), leftSubstPHI, ConstantPointerNull::get(TypeArgumentListStackType()->getPointerTo()) });
					subOfNull->setCallingConv(NOMCC);
					auto success = builder->CreateICmpEQ(subOfNull, MakeUInt(2, 3));
					builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { success, MakeUInt(1,0) });
					builder->CreateCondBr(success, successBlock, notSubOfNullBlock, GetLikelySecondBranchMetadata());

					builder->SetInsertPoint(notSubOfNullBlock);
					auto subOfOther = builder->CreateCall(fun, { leftPHI,  RTMaybeType::GenerateReadPotentialType(builder, rightPHI), leftSubstPHI, rightSubstPHI });
					subOfOther->setCallingConv(NOMCC);
					success = builder->CreateICmpEQ(subOfOther, MakeUInt(2, 3));
					builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { success, MakeUInt(1,1) });
					builder->CreateCondBr(success, successBlock, failBlock, GetLikelyFirstBranchMetadata());
				}

				BasicBlock* rightClsMergeBlock = BasicBlock::Create(LLVMCONTEXT, "rightClassInfoMerge", fun);
				builder->SetInsertPoint(rightClsMergeBlock);
				auto rightTypeArgsPHI = builder->CreatePHI(TYPETYPE->getPointerTo(), 2, "rightTypeArgs");
				auto rightIfacePHI = builder->CreatePHI(RTInterface::GetLLVMType()->getPointerTo(), 2, "rightInterfacePtr");

				if (rightClassTypeBlock != nullptr)
				{
					builder->SetInsertPoint(rightClassTypeBlock);
					rightTypeArgsPHI->addIncoming(RTClassType::GetTypeArgumentsPtr(builder, rightPHI), builder->GetInsertBlock());
					rightIfacePHI->addIncoming(RTClassType::GenerateReadClassDescriptorLink(builder, rightPHI), builder->GetInsertBlock());
					builder->CreateBr(rightClsMergeBlock);

				}
				if (rightInstanceTypeBlock != nullptr)
				{
					builder->SetInsertPoint(rightInstanceTypeBlock);
					rightTypeArgsPHI->addIncoming(RTInstanceType::GetTypeArgumentsPtr(builder, rightPHI), builder->GetInsertBlock());
					rightIfacePHI->addIncoming(RTInstanceType::GenerateReadClassDescriptorLink(builder, rightPHI), builder->GetInsertBlock());
					builder->CreateBr(rightClsMergeBlock);
				}

				builder->SetInsertPoint(rightClsMergeBlock);

				BasicBlock* foundNamedImmediateTypeMatch = BasicBlock::Create(LLVMCONTEXT, "foundNamedImmediateTypeMatch", fun);
				//BasicBlock* foundNamedImmediateTypeMatchLoadSubsts = BasicBlock::Create(LLVMCONTEXT, "foundNamedImmediateTypeMatch$loadSubsts", fun);
				BasicBlock* foundNamedTypeMatch = BasicBlock::Create(LLVMCONTEXT, "foundNamedTypeMatch", fun);
				BasicBlock* foundNamedTypeMatchLoad = BasicBlock::Create(LLVMCONTEXT, "foundNamedTypeMatch$Load", fun);
				BasicBlock* superTypeLoopHead = BasicBlock::Create(LLVMCONTEXT, "superTypeLoop$Head", fun);
				BasicBlock* superTypeLoopBody = BasicBlock::Create(LLVMCONTEXT, "superTypeLoop$Body", fun);
				BasicBlock* superTypeLoopBodyEnd = BasicBlock::Create(LLVMCONTEXT, "superTypeLoop$Body$End", fun);
				auto namedTypeMatch = builder->CreateICmpEQ(builder->CreatePtrToInt(leftIfacePHI, numtype(intptr_t)), builder->CreatePtrToInt(rightIfacePHI, numtype(intptr_t)));
				builder->CreateCondBr(namedTypeMatch, foundNamedImmediateTypeMatch, superTypeLoopHead);

				builder->SetInsertPoint(foundNamedImmediateTypeMatch);
				builder->CreateBr(foundNamedTypeMatch);

				builder->SetInsertPoint(foundNamedTypeMatch);
				BasicBlock* argCheckLoopHead = BasicBlock::Create(LLVMCONTEXT, "argCheckLoop$Head", fun);
				BasicBlock* argCheckLoopBody = BasicBlock::Create(LLVMCONTEXT, "argCheckLoop$Body", fun);

				PHINode* foundMatchTypeArgsPHI = builder->CreatePHI(leftTypeArgsPHI->getType(), 2);
				PHINode* foundSubstitutionsPHI = builder->CreatePHI(leftSubstPHI->getType(), 2);
				foundMatchTypeArgsPHI->addIncoming(leftTypeArgsPHI, foundNamedImmediateTypeMatch);
				foundSubstitutionsPHI->addIncoming(leftSubstPHI, foundNamedImmediateTypeMatch);
				auto ifaceArgCount = builder->CreateZExtOrTrunc(RTInterface::GenerateReadTypeArgCount(builder, leftIfacePHI), numtype(int32_t));
				auto ifaceArgPos = MakeIntLike(ifaceArgCount, 0);
				auto ifaceArgsLeft = builder->CreateICmpULT(ifaceArgPos, ifaceArgCount);
				builder->CreateCondBr(ifaceArgsLeft, argCheckLoopHead, successBlock);

				builder->SetInsertPoint(argCheckLoopHead);
				PHINode* ifaceArgPosPHI = builder->CreatePHI(ifaceArgPos->getType(), 2);
				ifaceArgPosPHI->addIncoming(ifaceArgPos, foundNamedTypeMatch);
				auto currentLeftArg = MakeLoad(builder, builder->CreateGEP(foundMatchTypeArgsPHI, builder->CreateSub(MakeInt32(-1), ifaceArgPosPHI)));
				auto currentRightArg = MakeLoad(builder, builder->CreateGEP(rightTypeArgsPHI, builder->CreateSub(MakeInt32(-1), ifaceArgPosPHI)));
				auto argsEqual = builder->CreateCall(RTTypeEq::Instance(false).GetLLVMElement(mod), { currentLeftArg, leftSubstPHI, currentRightArg, rightSubstPHI });
				argsEqual->setCallingConv(NOMCC);
				builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { argsEqual, MakeUInt(1,1) });
				builder->CreateCondBr(argsEqual, argCheckLoopBody, failBlock, GetLikelyFirstBranchMetadata());

				{
					builder->SetInsertPoint(argCheckLoopBody);
					auto newIndex = builder->CreateAdd(ifaceArgPosPHI, MakeInt32(1));
					ifaceArgsLeft = builder->CreateICmpULT(newIndex, ifaceArgCount);
					ifaceArgPosPHI->addIncoming(newIndex, builder->GetInsertBlock());
					builder->CreateCondBr(ifaceArgsLeft, argCheckLoopHead, successBlock);
				}
				builder->SetInsertPoint(superTypeLoopHead);
				auto superInstances = RTInterface::GenerateReadSuperInstances(builder, leftIfacePHI);
				auto superInstanceCount = RTInterface::GenerateReadSuperInstanceCount(builder, leftIfacePHI);
				auto currentIndex = MakeIntLike(superInstanceCount, 0);
				auto areInstancesLeft = builder->CreateICmpULT(currentIndex, superInstanceCount);
				builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { areInstancesLeft, MakeUInt(1,1) });
				builder->CreateCondBr(areInstancesLeft, superTypeLoopBody, failBlock, GetLikelyFirstBranchMetadata());

				builder->SetInsertPoint(superTypeLoopBody);
				auto indexPHI = builder->CreatePHI(currentIndex->getType(), 2, "superInstanceIndex");
				indexPHI->addIncoming(currentIndex, superTypeLoopHead);
				auto currentSuperIface = MakeLoad(builder, builder->CreateGEP(superInstances, { indexPHI, MakeInt32(SuperInstanceEntryFields::Class) }));
				namedTypeMatch = builder->CreateICmpEQ(builder->CreatePtrToInt(rightIfacePHI, numtype(intptr_t)), builder->CreatePtrToInt(currentSuperIface, numtype(intptr_t)));
				builder->CreateCondBr(namedTypeMatch, foundNamedTypeMatchLoad, superTypeLoopBodyEnd);

				builder->SetInsertPoint(foundNamedTypeMatchLoad);
				auto superInstanceTypeArgs = MakeLoad(builder, builder->CreateGEP(superInstances, { indexPHI, MakeInt32(SuperInstanceEntryFields::TypeArgs) }));
				auto newLeftSubstNode = builder->CreateAlloca(RTSubtyping::TypeArgumentListStackType(), MakeInt32(1));
				MakeStore(builder, leftSubstPHI, newLeftSubstNode, MakeInt32(TypeArgumentListStackFields::Next));
				MakeStore(builder, leftTypeArgsPHI, newLeftSubstNode, MakeInt32(TypeArgumentListStackFields::Types));
				foundMatchTypeArgsPHI->addIncoming(superInstanceTypeArgs, builder->GetInsertBlock());
				foundSubstitutionsPHI->addIncoming(newLeftSubstNode, builder->GetInsertBlock());
				builder->CreateBr(foundNamedTypeMatch);

				{
					builder->SetInsertPoint(superTypeLoopBodyEnd);
					auto newIndex = builder->CreateAdd(indexPHI, MakeIntLike(indexPHI, 1), "newIndex");
					indexPHI->addIncoming(newIndex, superTypeLoopBodyEnd);
					areInstancesLeft = builder->CreateICmpULT(newIndex, superInstanceCount);
					builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { areInstancesLeft, MakeUInt(1,1) });
					builder->CreateCondBr(areInstancesLeft, superTypeLoopBody, failBlock, GetLikelyFirstBranchMetadata());
				}
			}

			builder->SetInsertPoint(successBlock);
			builder->CreateRet(MakeUInt(2, 3));

			builder->SetInsertPoint(optimisticBlock);
			builder->CreateRet(MakeUInt(2, 1));

			builder->SetInsertPoint(failBlock);
			builder->CreateRet(MakeUInt(2, 0));

			llvm::raw_os_ostream out(std::cout);
			if (verifyFunction(*fun, &out))
			{
				out.flush();
				std::cout << "Could not verify RT_NOM_Subtyping!";
				fun->print(out);
				out.flush();
				std::cout.flush();
				throw new std::exception();
			}
			return fun;
		}



		llvm::Function* RTSubtyping::findLLVMElement(llvm::Module& mod) const
		{
			return mod.getFunction("NOM_RT_Subtyping");
		}

		//Arguments:
		//left type
		//right type,
		//left type argument list stack,
		//right type argument list stack
		llvm::FunctionType* RTSubtyping::SubtypingFunctionType()
		{
			static llvm::FunctionType* ft = llvm::FunctionType::get(inttype(2), { TYPETYPE, TYPETYPE, TypeArgumentListStackType()->getPointerTo(), TypeArgumentListStackType()->getPointerTo() }, false);
			return ft;
		}

		llvm::StructType* RTSubtyping::TypeArgumentListStackType()
		{
			static llvm::StructType* talst = llvm::StructType::create(LLVMCONTEXT, "NOM_RT_TypeArgumentListStack");
			static bool once = true;
			if (once)
			{
				once = false;
				talst->setBody(talst->getPointerTo(), TYPETYPE->getPointerTo());
			}
			return talst;
		}

		llvm::Value* RTSubtyping::CreateTypeSubtypingCheck(NomBuilder& builder, llvm::Module& mod, llvm::Value* left, llvm::Value* right, llvm::Value* leftsubstitutions, llvm::Value* rightsubstitutions)
		{
			auto stfun = RTSubtyping::Instance().GetLLVMElement(mod);
			auto callinst = builder->CreateCall(stfun, { left, right, leftsubstitutions, rightsubstitutions }, "isSubtype");
			callinst->setCallingConv(NOMCC);
			return callinst;
		}

		static void CreateInlineTypeArgSubChecks(NomBuilder& builder, llvm::Value* foundMatchTypeArgs, llvm::Value* leftSubstStack, NomClassTypeRef clsType, llvm::Value* rightSubstStack, BasicBlock* pessimisticBlock, BasicBlock* optimisticBlock, BasicBlock* failBlock)
		{
			auto fun = builder->GetInsertBlock()->getParent();
			int argIndex = 0;
			BasicBlock* nextCheck = builder->GetInsertBlock();
			BasicBlock* nextOptCheck = nullptr;
			for (auto* targ : clsType->Arguments)
			{
				argIndex++; //needs to be at least -1 anyway
				BasicBlock* curOptCheck = nextOptCheck;
				nextCheck = BasicBlock::Create(LLVMCONTEXT, "typeArg" + std::to_string(argIndex), fun);
				if (optimisticBlock != nullptr && optimisticBlock != pessimisticBlock)
				{
					nextOptCheck = BasicBlock::Create(LLVMCONTEXT, "typeArgOpt" + std::to_string(argIndex), fun);
				}
				auto argLeft = MakeInvariantLoad(builder, builder->CreateGEP(foundMatchTypeArgs, MakeInt32(-(argIndex))));
				RTTypeEq::CreateInlineTypeEqCheck(builder, argLeft, targ, leftSubstStack, rightSubstStack, nextCheck, optimisticBlock == pessimisticBlock ? nextCheck : nextOptCheck, failBlock);

				if (curOptCheck != nullptr)
				{
					builder->SetInsertPoint(curOptCheck);
					auto argLeft = MakeInvariantLoad(builder, builder->CreateGEP(foundMatchTypeArgs, MakeInt32(-(argIndex))));
					RTTypeEq::CreateInlineTypeEqCheck(builder, argLeft, targ, leftSubstStack, rightSubstStack, nextOptCheck, nextOptCheck, failBlock);
				}

				builder->SetInsertPoint(nextCheck);
			}
			builder->CreateBr(pessimisticBlock);

			if (nextOptCheck != nullptr)
			{
				builder->SetInsertPoint(nextOptCheck);
				builder->CreateBr(optimisticBlock);
			}
		}

		void RTSubtyping::CreateInlineSubtypingCheck(NomBuilder& builder, llvm::Value* leftType, llvm::Value* leftSubst, NomTypeRef rightType, llvm::Value* rightSubstitutions, BasicBlock* pessimisticBlock, BasicBlock* optimisticBlock, BasicBlock* failBlock)
		{
			BasicBlock* origBlock = builder->GetInsertBlock();
			Function* fun = origBlock->getParent();
			if (leftSubst == nullptr)
			{
				leftSubst = ConstantPointerNull::get(RTSubstStack::GetLLVMType()->getPointerTo());
			}
			if (rightSubstitutions == nullptr)
			{
				rightSubstitutions = ConstantPointerNull::get(RTSubstStack::GetLLVMType()->getPointerTo());
			}

			switch (rightType->GetKind())
			{
			case TypeKind::TKTop:
			case TypeKind::TKDynamic:
				builder->CreateBr(pessimisticBlock);
				break;
			case TypeKind::TKVariable:
			{
				auto rightTypeSubstList = MakeInvariantLoad(builder, rightSubstitutions, MakeInt32(TypeArgumentListStackFields::Types), "", AtomicOrdering::NotAtomic);
				auto rightTypeSubst = MakeInvariantLoad(builder, builder->CreateGEP(rightTypeSubstList, MakeInt32(-(((NomTypeVarRef)rightType)->GetIndex() + 1))), "", AtomicOrdering::NotAtomic);
				auto newSubsts = MakeInvariantLoad(builder, rightSubstitutions, MakeInt32(TypeArgumentListStackFields::Next));
				Function* subtypingFun = RTSubtyping::Instance().GetLLVMElement(*fun->getParent());
				auto subtypingResult = builder->CreateCall(subtypingFun, { leftType, rightTypeSubst, leftSubst, newSubsts });
				subtypingResult->setCallingConv(NOMCC);
				if (optimisticBlock == nullptr)
				{
					auto success = builder->CreateICmpEQ(subtypingResult, MakeUInt(2, 3));
					builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { success, MakeUInt(1,1) });
					builder->CreateCondBr(success, pessimisticBlock, failBlock, GetLikelyFirstBranchMetadata());
				}
				else if (optimisticBlock == pessimisticBlock)
				{
					auto fail = builder->CreateICmpEQ(subtypingResult, MakeUInt(2, 0));
					builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { fail, MakeUInt(1,0) });
					builder->CreateCondBr(fail, failBlock, optimisticBlock, GetLikelySecondBranchMetadata());
				}
				else
				{
					auto successSwitch = builder->CreateSwitch(subtypingResult, failBlock, 2, GetBranchWeights({0,100,50}));
					successSwitch->addCase(MakeUInt(2, 3), pessimisticBlock);
					successSwitch->addCase(MakeUInt(2, 1), optimisticBlock);
				}
			}
			break;
			case TypeKind::TKClass:
			{
				auto clsType = (NomClassTypeRef)rightType;
				BasicBlock* classTypeBlock = nullptr, * instanceTypeBlock = nullptr;
				Value* leftTypePHI = nullptr, * leftSubstPHI = nullptr;

				RTTypeHead::GenerateTypeKindSwitchRecurse(builder, leftType, leftSubst, &leftTypePHI, &leftSubstPHI, &classTypeBlock, &failBlock, &failBlock, &pessimisticBlock, &instanceTypeBlock, (optimisticBlock == nullptr ? &failBlock : &optimisticBlock), &failBlock, failBlock);

				BasicBlock* clsMergeBlock = BasicBlock::Create(LLVMCONTEXT, "classInfoMerge", fun);
				builder->SetInsertPoint(clsMergeBlock);
				auto typeArgsPHI = builder->CreatePHI(TYPETYPE->getPointerTo(), 2, "typeArgs");
				auto ifacePHI = builder->CreatePHI(RTInterface::GetLLVMType()->getPointerTo(), 2, "interfacePtr");

				if (classTypeBlock != nullptr)
				{
					builder->SetInsertPoint(classTypeBlock);
					typeArgsPHI->addIncoming(RTClassType::GetTypeArgumentsPtr(builder, leftTypePHI), builder->GetInsertBlock());
					ifacePHI->addIncoming(RTClassType::GenerateReadClassDescriptorLink(builder, leftTypePHI), builder->GetInsertBlock());
					builder->CreateBr(clsMergeBlock);

				}
				if (instanceTypeBlock != nullptr)
				{
					builder->SetInsertPoint(instanceTypeBlock);
					typeArgsPHI->addIncoming(RTInstanceType::GetTypeArgumentsPtr(builder, leftTypePHI), builder->GetInsertBlock());
					ifacePHI->addIncoming(RTInstanceType::GenerateReadClassDescriptorLink(builder, leftTypePHI), builder->GetInsertBlock());
					builder->CreateBr(clsMergeBlock);
				}

				builder->SetInsertPoint(clsMergeBlock);

				BasicBlock* foundNamedTypeMatch = BasicBlock::Create(LLVMCONTEXT, "foundNamedTypeMatch", fun);
				BasicBlock* foundNamedTypeMatchLoad = BasicBlock::Create(LLVMCONTEXT, "foundNamedTypeMatch$Load", fun);
				BasicBlock* superTypeLoopHead = BasicBlock::Create(LLVMCONTEXT, "superTypeLoop$Head", fun);
				BasicBlock* superTypeLoopBody = BasicBlock::Create(LLVMCONTEXT, "superTypeLoop$Body", fun);
				BasicBlock* superTypeLoopBodyEnd = BasicBlock::Create(LLVMCONTEXT, "superTypeLoop$Body$End", fun);
				auto namedTypeMatch = builder->CreateICmpEQ(ConstantExpr::getPtrToInt(clsType->Named->GetInterfaceDescriptor(*fun->getParent()), numtype(intptr_t)), builder->CreatePtrToInt(ifacePHI, numtype(intptr_t)));
				builder->CreateCondBr(namedTypeMatch, foundNamedTypeMatch, superTypeLoopHead);

				builder->SetInsertPoint(foundNamedTypeMatch);
				CreateInlineTypeArgSubChecks(builder, typeArgsPHI, leftSubstPHI, clsType, rightSubstitutions, pessimisticBlock, optimisticBlock, failBlock);


				builder->SetInsertPoint(superTypeLoopHead);
				auto superInstances = RTInterface::GenerateReadSuperInstances(builder, ifacePHI);
				auto superInstanceCount = RTInterface::GenerateReadSuperInstanceCount(builder, ifacePHI);
				auto currentIndex = MakeIntLike(superInstanceCount, 0);
				auto areInstancesLeft = builder->CreateICmpULT(currentIndex, superInstanceCount);
				builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { areInstancesLeft, MakeUInt(1,1) });
				builder->CreateCondBr(areInstancesLeft, superTypeLoopBody, failBlock, GetLikelyFirstBranchMetadata());

				builder->SetInsertPoint(superTypeLoopBody);
				auto indexPHI = builder->CreatePHI(currentIndex->getType(), 2, "superInstanceIndex");
				indexPHI->addIncoming(currentIndex, superTypeLoopHead);
				auto currentSuperIface = MakeInvariantLoad(builder, builder->CreateGEP(superInstances, { indexPHI, MakeInt32(SuperInstanceEntryFields::Class) }));
				namedTypeMatch = builder->CreateICmpEQ(ConstantExpr::getPtrToInt(clsType->Named->GetInterfaceDescriptor(*fun->getParent()), numtype(intptr_t)), builder->CreatePtrToInt(currentSuperIface, numtype(intptr_t)));
				builder->CreateCondBr(namedTypeMatch, foundNamedTypeMatchLoad, superTypeLoopBodyEnd);

				builder->SetInsertPoint(foundNamedTypeMatchLoad);
				auto superInstanceTypeArgs = MakeInvariantLoad(builder, builder->CreateGEP(superInstances, { indexPHI, MakeInt32(SuperInstanceEntryFields::TypeArgs) }));
				RTSubstStackValue newLeftSubst = RTSubstStackValue(builder, typeArgsPHI, leftSubstPHI);
				BasicBlock* releasePessimisticBlock = nullptr, * releaseFailBlock = nullptr, * releaseOptimisticBlock = nullptr;
				newLeftSubst.MakeReleaseBlocks(builder, pessimisticBlock, &releasePessimisticBlock, failBlock, &releaseFailBlock, optimisticBlock, &releaseOptimisticBlock);
				CreateInlineTypeArgSubChecks(builder, superInstanceTypeArgs, newLeftSubst, clsType, rightSubstitutions, releasePessimisticBlock, releaseOptimisticBlock, releaseFailBlock);

				builder->SetInsertPoint(superTypeLoopBodyEnd);
				auto newIndex = builder->CreateAdd(indexPHI, MakeIntLike(indexPHI, 1), "newIndex");
				indexPHI->addIncoming(newIndex, superTypeLoopBodyEnd);
				areInstancesLeft = builder->CreateICmpULT(newIndex, superInstanceCount);
				builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { areInstancesLeft, MakeUInt(1,1) });
				builder->CreateCondBr(areInstancesLeft, superTypeLoopBody, failBlock, GetLikelyFirstBranchMetadata());

			}
			break;
			case TypeKind::TKMaybe:
			{
				if (optimisticBlock == nullptr)
				{
					optimisticBlock = failBlock;
				}
				BasicBlock* maybeBlock = nullptr;
				Value* leftPHI = nullptr;
				Value* leftSubstPHI = nullptr;
				RTTypeHead::GenerateTypeKindSwitchRecurse(builder, leftType, leftSubst, &leftPHI, &leftSubstPHI, &failBlock, &failBlock, &failBlock, &failBlock, &failBlock, &optimisticBlock, &maybeBlock, failBlock);

				if (maybeBlock != nullptr)
				{
					builder->SetInsertPoint(maybeBlock);
					BasicBlock* notNullBlock = BasicBlock::Create(LLVMCONTEXT, "leftTypeNotNull", fun);
					auto isNullType = CreatePointerEq(builder, leftPHI, NomNullClass::GetInstance()->GetType()->GetLLVMElement(*fun->getParent()));
					builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { isNullType, MakeUInt(1,0) });
					builder->CreateCondBr(isNullType, pessimisticBlock, notNullBlock, GetLikelySecondBranchMetadata());

					builder->SetInsertPoint(notNullBlock);
					RTTypeEq::CreateInlineTypeEqCheck(builder, leftPHI, ((NomMaybeTypeRef)rightType)->PotentialType, leftSubstPHI, rightSubstitutions, pessimisticBlock, optimisticBlock, failBlock);
				}
			}
			break;
			default:
				throw new std::exception(); //the other type kinds should not show up here
			}
		}


	}
}