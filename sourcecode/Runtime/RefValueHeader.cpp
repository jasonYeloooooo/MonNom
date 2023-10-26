#include "RefValueHeader.h"
#include "Context.h"
#include "RTVTable.h"
#include "Defs.h"
#include "llvm/IR/Constants.h"
#include "CompileHelpers.h"
#include "RTOutput.h"
#include "StringClass.h"
#include <iostream>
#include "llvm/Support/raw_os_ostream.h"
#include "RTInterfaceTableEntry.h"
#include "IntClass.h"
#include "FloatClass.h"
#include "CallingConvConf.h"
#include "IMT.h"
#include "RTCompileConfig.h"
#include "BoolClass.h"
#include "RecordHeader.h"
#include "NomClassType.h"
#include "StructuralValueHeader.h"
#include "Metadata.h"
#include "NomMaybeType.h"

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{
		llvm::StructType* RefValueHeader::GetUninitializedLLVMType()
		{
			static llvm::StructType* rvht = llvm::StructType::create(LLVMCONTEXT, "RT_NOM_RefValueHeader");
			return rvht;
		}
		llvm::StructType* RefValueHeader::GetLLVMType()
		{
			static llvm::StructType* rvht = RefValueHeader::GetUninitializedLLVMType();
			static bool once = true;
			if (once)
			{
				once = false;
				rvht->setBody(
					RTVTable::GetLLVMType()->getPointerTo(),	// vtable pointer
					arrtype(POINTERTYPE, 0)						// used for raw invokes with lambda optimziation (for invokable objects, the first field is let free;
																// for structural values, a 64-bit buffer is left between vtable and s-table pointer; in any case
																// accesses to raw invokes should always run through this field)
				);
			}
			return rvht;
		}
		llvm::Value* RefValueHeader::GenerateReadVTablePointer(NomBuilder& builder, llvm::Value* refValue)
		{
			return MakeInvariantLoad(builder, builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), MakeInt32(RefValueHeaderFields::InterfaceTable));
		}

		MDNode* GetNominalVsStructuralBranchWeights()
		{
			static MDNode* mdn = MDNode::get(LLVMCONTEXT, { MDString::get(LLVMCONTEXT, "branch_weights"), ConstantAsMetadata::get(MakeInt32(1)), ConstantAsMetadata::get(MakeInt32(9)) });
			return mdn;
		}

		int RefValueHeader::GenerateRefOrPrimitiveValueSwitch(NomBuilder& builder, NomValue value, llvm::BasicBlock** refValueBlock, llvm::BasicBlock** intBlock, llvm::BasicBlock** floatBlock, int refWeight, int intWeight, int floatWeight, int boolWeight)
		{
			return GenerateRefOrPrimitiveValueSwitch(builder, value, refValueBlock, intBlock, floatBlock, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, refWeight, intWeight, floatWeight, boolWeight);
		}

		int RefValueHeader::GenerateNominalStructuralSwitch(NomBuilder& builder, NomValue refValue, llvm::Value** vTableVar, llvm::BasicBlock** nominalObjectBlockVar, llvm::BasicBlock** structuralValueBlockVar)
		{
			if (vTableVar != nullptr && *vTableVar == nullptr)
			{
				*vTableVar = RefValueHeader::GenerateReadVTablePointer(builder, refValue);
			}
			if ((refValue.GetNomType()->GetKind() == TypeKind::TKClass && !(((NomClassTypeRef)refValue.GetNomType())->Named->IsInterface())) || (refValue.GetNomType()->GetKind() == TypeKind::TKMaybe && ((NomMaybeTypeRef)refValue.GetNomType())->PotentialType->GetKind() == TypeKind::TKClass && !(((NomClassTypeRef)((NomMaybeTypeRef)refValue.GetNomType())->PotentialType)->Named->IsFunctional())))
			{
				if (nominalObjectBlockVar == nullptr)
				{
					throw new std::exception();
				}
				if (*nominalObjectBlockVar != nullptr)
				{
					builder->CreateBr(*nominalObjectBlockVar);
				}
				else
				{
					*nominalObjectBlockVar = builder->GetInsertBlock();
				}
				return 1;
			}
			if (refValue.GetNomType()->GetKind() == TypeKind::TKRecord || refValue.GetNomType()->GetKind() == TypeKind::TKLambda || refValue.GetNomType()->GetKind() == TypeKind::TKPartialApp)
			{
				if (structuralValueBlockVar == nullptr)
				{
					throw new std::exception();
				}
				if (*structuralValueBlockVar != nullptr)
				{
					builder->CreateBr(*structuralValueBlockVar);
				}
				else
				{
					*structuralValueBlockVar = builder->GetInsertBlock();
				}
				return 1;
			}
			Value* vtable = nullptr;
			if (vTableVar != nullptr)
			{
				vtable = *vTableVar;
			}
			else
			{
				vtable = RefValueHeader::GenerateReadVTablePointer(builder, refValue);
			}
			Function* fun = builder->GetInsertBlock()->getParent();
			if (nominalObjectBlockVar == nullptr || structuralValueBlockVar == nullptr)
			{
				throw new std::exception();
			}
			if (*nominalObjectBlockVar == nullptr)
			{
				*nominalObjectBlockVar = BasicBlock::Create(LLVMCONTEXT, "nominalObject", builder->GetInsertBlock()->getParent());
			}
			if (*structuralValueBlockVar == nullptr)
			{
				*structuralValueBlockVar = BasicBlock::Create(LLVMCONTEXT, "structuralValue", builder->GetInsertBlock()->getParent());
			}
			auto vTableIsNominal = RTVTable::GenerateIsNominalValue(builder, vtable);
			builder->CreateIntrinsic(llvm::Intrinsic::expect, inttype(1), { vTableIsNominal, MakeUInt(1,1) });
			builder->CreateCondBr(vTableIsNominal, *nominalObjectBlockVar, *structuralValueBlockVar, GetLikelyFirstBranchMetadata());
			return 2;
		}

		int RefValueHeader::GenerateStructuralValueKindSwitch(NomBuilder& builder, NomValue refValue, llvm::Value** vtableVar, llvm::BasicBlock** lambdaBlock, llvm::BasicBlock** recordBlock, llvm::BasicBlock** partialAppBlock)
		{
			auto fun = builder->GetInsertBlock()->getParent();
			if (vtableVar != nullptr && *vtableVar == nullptr)
			{
				*vtableVar = RefValueHeader::GenerateReadVTablePointer(builder, refValue);
			}
			if (lambdaBlock == nullptr || recordBlock == nullptr || partialAppBlock == nullptr)
			{
				throw new std::exception();
			}
			if (refValue.GetNomType()->GetKind() == TypeKind::TKLambda)
			{
				if (*lambdaBlock == nullptr)
				{
					*lambdaBlock = builder->GetInsertBlock();
				}
				else
				{
					builder->CreateBr(*lambdaBlock);
				}
				return 1;
			}
			if (refValue.GetNomType()->GetKind() == TypeKind::TKRecord)
			{
				if (*recordBlock == nullptr)
				{
					*recordBlock = builder->GetInsertBlock();
				}
				else
				{
					builder->CreateBr(*recordBlock);
				}
				return 1;
			}
			if (refValue.GetNomType()->GetKind() == TypeKind::TKPartialApp)
			{
				if (*partialAppBlock == nullptr)
				{
					*partialAppBlock = builder->GetInsertBlock();
				}
				else
				{
					builder->CreateBr(*partialAppBlock);
				}
				return 1;
			}
			if (*lambdaBlock == nullptr)
			{
				*lambdaBlock = BasicBlock::Create(LLVMCONTEXT, "isLambda", fun);
			}
			if (*recordBlock == nullptr)
			{
				*recordBlock = BasicBlock::Create(LLVMCONTEXT, "isRecord", fun);
			}
			if (*partialAppBlock == nullptr)
			{
				*partialAppBlock = BasicBlock::Create(LLVMCONTEXT, "isPartialApp", fun);
			}

			Value* vtable = nullptr;
			if (vtableVar != nullptr)
			{
				vtable = *vtableVar;
			}
			else
			{
				vtable = RefValueHeader::GenerateReadVTablePointer(builder, refValue);
			}
			auto vtableKind = RTVTable::GenerateReadKind(builder, vtable);
			auto switchInst = builder->CreateSwitch(vtableKind, *partialAppBlock, 2, GetBranchWeightsForBlocks({ *partialAppBlock, *lambdaBlock, *recordBlock }));
			switchInst->addCase(MakeIntLike(vtableKind, (char)RTDescriptorKind::Lambda), *lambdaBlock);
			switchInst->addCase(MakeIntLike(vtableKind, (char)RTDescriptorKind::Record), *recordBlock);
			return 3;
		}

		int RefValueHeader::GenerateRefValueKindSwitch(NomBuilder& builder, NomValue refValue, llvm::Value** vtableVar, llvm::BasicBlock** nominalObjectBlock, llvm::BasicBlock** lambdaBlock, llvm::BasicBlock** recordBlock, llvm::BasicBlock** partialAppBlock)
		{
			BasicBlock* structuralObjectBlock = nullptr;
			int ret = GenerateNominalStructuralSwitch(builder, refValue, vtableVar, nominalObjectBlock, &structuralObjectBlock);
			if (structuralObjectBlock != nullptr)
			{
				builder->SetInsertPoint(structuralObjectBlock);
				return ret - 1 + GenerateStructuralValueKindSwitch(builder, refValue, vtableVar, lambdaBlock, recordBlock, partialAppBlock);
			}
			return 1;
		}

		void RefValueHeader::GenerateValueKindSwitch(NomBuilder& builder, NomValue value, llvm::Value** vtableVar, llvm::BasicBlock** nominalObjectBlock, llvm::BasicBlock** structuralValueBlock, llvm::BasicBlock** intBlock, llvm::BasicBlock** floatBlock, bool unpackPrimitives, llvm::BasicBlock** primitiveIntBlock, llvm::Value** primitiveIntValVar, llvm::BasicBlock** primitiveFloatBlock, llvm::Value** primitiveFloatValVar, llvm::BasicBlock** primitiveBoolBlock, llvm::Value** primitiveBoolValVar)
		{
			BasicBlock* refValueBlock = nullptr;
			RefValueHeader::GenerateRefOrPrimitiveValueSwitch(builder, value, &refValueBlock, intBlock, floatBlock, unpackPrimitives, primitiveIntBlock, primitiveIntValVar, primitiveFloatBlock, primitiveFloatValVar, primitiveBoolBlock, primitiveBoolValVar);

			if (refValueBlock != nullptr)
			{
				builder->SetInsertPoint(refValueBlock);
				RefValueHeader::GenerateNominalStructuralSwitch(builder, value, vtableVar, nominalObjectBlock, structuralValueBlock);
			}
		}
		void RefValueHeader::GenerateValueKindSwitch(NomBuilder& builder, NomValue value, llvm::Value** vtableVar, llvm::BasicBlock** nominalObjectBlock, llvm::BasicBlock** lambdaBlock, llvm::BasicBlock** recordBlock, llvm::BasicBlock** partialAppBlock, llvm::BasicBlock** intBlock, llvm::BasicBlock** floatBlock, bool unpackPrimitives, llvm::BasicBlock** primitiveIntBlock, llvm::Value** primitiveIntValVar, llvm::BasicBlock** primitiveFloatBlock, llvm::Value** primitiveFloatValVar, llvm::BasicBlock** primitiveBoolBlock, llvm::Value** primitiveBoolValVar)
		{
			BasicBlock* refValueBlock = nullptr;
			RefValueHeader::GenerateRefOrPrimitiveValueSwitch(builder, value, &refValueBlock, intBlock, floatBlock, unpackPrimitives, primitiveIntBlock, primitiveIntValVar, primitiveFloatBlock, primitiveFloatValVar, primitiveBoolBlock, primitiveBoolValVar);

			if (refValueBlock != nullptr)
			{
				builder->SetInsertPoint(refValueBlock);
				RefValueHeader::GenerateRefValueKindSwitch(builder, value, vtableVar, nominalObjectBlock, lambdaBlock, recordBlock, partialAppBlock);
			}
		}

		int RefValueHeader::GenerateRefOrPrimitiveValueSwitch(NomBuilder& builder, NomValue value, llvm::BasicBlock** refValueBlock, llvm::BasicBlock** intBlock, llvm::BasicBlock** floatBlock, bool unpackPrimitives, llvm::BasicBlock** primitiveIntBlock, llvm::Value** primitiveIntVar, llvm::BasicBlock** primitiveFloatBlock, llvm::Value** primitiveFloatVar, llvm::BasicBlock** primitiveBoolBlock, llvm::Value** primitiveBoolVar, int refWeight, int intWeight, int floatWeight, int boolWeight)
		{
			BasicBlock* origBlock = builder->GetInsertBlock();
			Function* fun = origBlock->getParent();
			llvm::Type* valType = value->getType();
			if (valType->isIntegerTy(INTTYPE->getIntegerBitWidth()))
			{
				if (primitiveIntBlock == nullptr)
				{
					throw new std::exception();
				}
				*primitiveIntBlock = builder->GetInsertBlock();
				if (primitiveIntVar != nullptr)
				{
					*primitiveIntVar = *value;
				}
				return 1;
			}
			if (valType->isDoubleTy())
			{
				if (primitiveFloatBlock == nullptr)
				{
					throw new std::exception();
				}
				*primitiveFloatBlock = builder->GetInsertBlock();
				if (primitiveFloatVar != nullptr)
				{
					*primitiveFloatVar = *value;
				}
				return 1;
			}
			if (valType->isIntegerTy(BOOLTYPE->getIntegerBitWidth()))
			{
				if (primitiveBoolBlock == nullptr)
				{
					throw new std::exception();
				}
				*primitiveBoolBlock = builder->GetInsertBlock();
				if (primitiveBoolVar != nullptr)
				{
					*primitiveBoolVar = *value;
				}
				return 1;
			}

			if (unpackPrimitives && value.GetNomType()->IsSubtype(GetIntClassType(), false))
			{
				if (primitiveIntBlock == nullptr || primitiveIntVar == nullptr)
				{
					throw new std::exception();
				}
				*primitiveIntVar = UnpackInt(builder, value);
				*primitiveIntBlock = builder->GetInsertBlock();
				return 1;
			}
			else if (unpackPrimitives && value.GetNomType()->IsSubtype(GetFloatClassType(), false))
			{
				if (primitiveFloatBlock == nullptr || primitiveFloatVar == nullptr)
				{
					throw new std::exception();
				}
				*primitiveFloatVar = UnpackFloat(builder, value);
				*primitiveFloatBlock = builder->GetInsertBlock();
				return 1;
			}
			else if (unpackPrimitives && value.GetNomType()->IsSubtype(GetBoolClassType(), false))
			{
				if (primitiveBoolBlock == nullptr || primitiveBoolVar == nullptr)
				{
					throw new std::exception();
				}
				*primitiveBoolVar = UnpackBool(builder, value);
				*primitiveBoolBlock = builder->GetInsertBlock();
				return 1;
			}

			bool intsPossible = !value.GetNomType()->IsDisjoint(GetIntClassType());
			bool floatsPossible = !value.GetNomType()->IsDisjoint(GetFloatClassType());
			bool boolsPossible = !value.GetNomType()->IsDisjoint(GetFloatClassType());

			int cases = 1; //could always be normal reference at this point

			BasicBlock* _packedIntBlock = nullptr;
			BasicBlock* _primitiveIntBlock = nullptr;
			PHINode* _primitiveIntPHI = nullptr;
			if (intBlock != nullptr)
			{
				_packedIntBlock = *intBlock;
			}
			if (primitiveIntBlock != nullptr)
			{
				_primitiveIntBlock = *primitiveIntBlock;
			}

			if (intsPossible)
			{
				cases += 1;
				if (_packedIntBlock == nullptr)
				{
					_packedIntBlock = BasicBlock::Create(LLVMCONTEXT, "packedInt", fun);
					if (intBlock != nullptr)
					{
						*intBlock = _packedIntBlock;
					}
					else if (!unpackPrimitives)
					{
						std::cout << "Internal error: did not cover packed integer case";
						throw new std::exception();
					}
				}
				if (unpackPrimitives)
				{
					if (primitiveIntBlock == nullptr || primitiveIntVar == nullptr)
					{
						throw new std::exception();
					}
					if (_primitiveIntBlock == nullptr)
					{
						_primitiveIntBlock = BasicBlock::Create(LLVMCONTEXT, "primitiveInt", fun);
						*primitiveIntBlock = _primitiveIntBlock;
					}
					if (_primitiveIntPHI == nullptr)
					{
						builder->SetInsertPoint(_primitiveIntBlock);
						_primitiveIntPHI = builder->CreatePHI(INTTYPE, 2, "primitiveIntValue");
						*primitiveIntVar = _primitiveIntPHI;
					}
					builder->SetInsertPoint(_packedIntBlock);
					auto unpackedInt = UnpackMaskedInt(builder, builder->CreatePtrToInt(value, INTTYPE, "refAsInt"));
					_primitiveIntPHI->addIncoming(unpackedInt, builder->GetInsertBlock());
					builder->CreateBr(_primitiveIntBlock);
				}
			}

			BasicBlock* _maskedFloatBlock = nullptr;
			BasicBlock* _primitiveFloatBlock = nullptr;
			PHINode* _primitiveFloatPHI = nullptr;
			if (floatBlock != nullptr)
			{
				_maskedFloatBlock = *floatBlock;
			}
			if (primitiveFloatBlock != nullptr)
			{
				_primitiveFloatBlock = *primitiveFloatBlock;
			}

			if (floatsPossible)
			{
				cases += 1;
				if (_maskedFloatBlock == nullptr)
				{
					_maskedFloatBlock = BasicBlock::Create(LLVMCONTEXT, "packedFloat", fun);
					if (floatBlock != nullptr)
					{
						*floatBlock = _maskedFloatBlock;
					}
					else if (!unpackPrimitives)
					{
						std::cout << "Internal error: did not cover packed float case";
						throw new std::exception();
					}
				}
				if (unpackPrimitives)
				{
					if (primitiveFloatBlock == nullptr || primitiveFloatVar == nullptr)
					{
						throw new std::exception();
					}
					if (_primitiveFloatBlock == nullptr)
					{
						_primitiveFloatBlock = BasicBlock::Create(LLVMCONTEXT, "primitiveFloat", fun);
						*primitiveFloatBlock = _primitiveFloatBlock;
					}
					if (_primitiveFloatPHI == nullptr)
					{
						builder->SetInsertPoint(_primitiveFloatBlock);
						_primitiveFloatPHI = builder->CreatePHI(FLOATTYPE, 2, "primitiveFloatValue");
						*primitiveFloatVar = _primitiveFloatPHI;
					}
					builder->SetInsertPoint(_maskedFloatBlock);
					auto unpackedFloat = UnpackMaskedFloat(builder, builder->CreatePtrToInt(value, INTTYPE, "refAsInt"));
					_primitiveFloatPHI->addIncoming(unpackedFloat, builder->GetInsertBlock());
					builder->CreateBr(_primitiveFloatBlock);
				}
			}

			if (refValueBlock == nullptr)
			{
				throw new std::exception();
			}

			llvm::Value* vtableAsInt = nullptr;
			if (cases == 1)
			{
				if ((!unpackPrimitives) || (!boolsPossible))
				{
					*refValueBlock = builder->GetInsertBlock();
					return 1;
				}
				else // booleans need to be unpacked to primitive values
				{
					if (primitiveBoolBlock == nullptr || primitiveBoolVar == nullptr)
					{
						throw new std::exception();
					}
					*refValueBlock = BasicBlock::Create(LLVMCONTEXT, "refValue", fun);
					*primitiveBoolBlock = BasicBlock::Create(LLVMCONTEXT, "boxedBooleanA", fun);
					vtableAsInt = builder->CreatePtrToInt(RefValueHeader::GenerateReadVTablePointer(builder, value), numtype(intptr_t));
					auto boolAsInt = ConstantExpr::getPtrToInt(NomBoolClass::GetInstance()->GetLLVMElement(*fun->getParent()), numtype(intptr_t));
					auto isBool = builder->CreateICmpEQ(vtableAsInt, boolAsInt);
					builder->CreateCondBr(isBool, *primitiveBoolBlock, *refValueBlock);
					builder->SetInsertPoint(*primitiveBoolBlock);
					*primitiveBoolVar = UnpackBool(builder, value);
					return 2;
				}
			}
			builder->SetInsertPoint(origBlock);
			auto refAsInt = builder->CreatePtrToInt(value, INTTYPE, "refAsInt");
			auto tag = builder->CreateTrunc(refAsInt, inttype(2), "tag");
			BasicBlock* _refValueBlock = BasicBlock::Create(LLVMCONTEXT, "refValue", fun);
			auto boolRefWeight = (boolWeight > refWeight ? boolWeight : refWeight);
			if ((!intsPossible) || (!floatsPossible))
			{
				bool refsMoreLikely = intsPossible ? (boolRefWeight >= intWeight) : (boolRefWeight >= floatWeight);
				MDNode* branchWeights = refsMoreLikely ? GetLikelyFirstBranchMetadata() : GetLikelySecondBranchMetadata();
				auto isRef = builder->CreateICmpEQ(tag, MakeIntLike(tag, 0), "isRef");
				CreateExpect(builder, isRef, MakeIntLike(isRef, refsMoreLikely ? 1 : 0));
				builder->CreateCondBr(isRef, _refValueBlock, intsPossible ? _packedIntBlock : _maskedFloatBlock, branchWeights);
			}
			else
			{
				uint64_t caseweights[3] = { (uint64_t)floatWeight, (uint64_t)boolRefWeight, (uint64_t)intWeight };
				SwitchInst* tagSwitch = builder->CreateSwitch(tag, _maskedFloatBlock, 3, GetBranchWeights(ArrayRef<uint64_t>(caseweights, 3)));
				tagSwitch->addCase(MakeUInt(2, 0), _refValueBlock);
				tagSwitch->addCase(MakeUInt(2, 3), _packedIntBlock);
			}
			if (unpackPrimitives)
			{
				builder->SetInsertPoint(_refValueBlock);
				vtableAsInt = builder->CreatePtrToInt(RefValueHeader::GenerateReadVTablePointer(builder, value), numtype(intptr_t));
				if (intsPossible)
				{
					BasicBlock* unboxIntBlock = BasicBlock::Create(LLVMCONTEXT, "unboxInt", fun);
					builder->SetInsertPoint(_refValueBlock);
					_refValueBlock = BasicBlock::Create(LLVMCONTEXT, "nonIntRefValue", fun);
					auto intAsInt = ConstantExpr::getPtrToInt(NomIntClass::GetInstance()->GetLLVMElement(*fun->getParent()), numtype(intptr_t));
					auto isInt = builder->CreateICmpEQ(vtableAsInt, intAsInt);
					CreateExpect(builder, isInt, MakeIntLike(isInt, intWeight > boolRefWeight && intWeight > floatWeight));
					builder->CreateCondBr(isInt, unboxIntBlock, _refValueBlock, (intWeight > boolRefWeight && intWeight > floatWeight)?GetLikelyFirstBranchMetadata():GetLikelySecondBranchMetadata());

					builder->SetInsertPoint(unboxIntBlock);
					auto unboxedInt = builder->CreatePtrToInt(ObjectHeader::ReadField(builder, value, MakeInt32(0), false), INTTYPE, "unboxedInt");
					_primitiveIntPHI->addIncoming(unboxedInt, unboxIntBlock);
					builder->CreateBr(_primitiveIntBlock);
				}
				if (floatsPossible)
				{
					BasicBlock* unboxFloatBlock = BasicBlock::Create(LLVMCONTEXT, "unboxInt", fun);
					builder->SetInsertPoint(_refValueBlock);
					_refValueBlock = BasicBlock::Create(LLVMCONTEXT, "nonIntNonFloatRefValue", fun);
					auto floatAsInt = ConstantExpr::getPtrToInt(NomFloatClass::GetInstance()->GetLLVMElement(*fun->getParent()), numtype(intptr_t));
					auto isFloat = builder->CreateICmpEQ(vtableAsInt, floatAsInt);
					CreateExpect(builder, isFloat, MakeIntLike(isFloat, floatWeight > boolRefWeight));
					builder->CreateCondBr(isFloat, unboxFloatBlock, _refValueBlock, (floatWeight > boolRefWeight) ? GetLikelyFirstBranchMetadata() : GetLikelySecondBranchMetadata());

					builder->SetInsertPoint(unboxFloatBlock);
					auto unboxedFloat = builder->CreateBitCast(builder->CreatePtrToInt(ObjectHeader::ReadField(builder, value, MakeInt32(0), false), INTTYPE), FLOATTYPE, "unboxedFloat");
					_primitiveFloatPHI->addIncoming(unboxedFloat, unboxFloatBlock);
					builder->CreateBr(_primitiveFloatBlock);
				}
				if (boolsPossible)
				{
					if (primitiveBoolBlock == nullptr || primitiveBoolVar == nullptr)
					{
						throw new std::exception();
					}
					builder->SetInsertPoint(_refValueBlock);
					_refValueBlock = BasicBlock::Create(LLVMCONTEXT, "nonIntNonFloatNonBoolRefValue", fun);
					*primitiveBoolBlock = BasicBlock::Create(LLVMCONTEXT, "boxedBoolean", fun);
					auto boolAsInt = ConstantExpr::getPtrToInt(NomBoolClass::GetInstance()->GetLLVMElement(*fun->getParent()), numtype(intptr_t));
					auto isBool = builder->CreateICmpEQ(vtableAsInt, boolAsInt);
					CreateExpect(builder, isBool, MakeIntLike(isBool, boolWeight > refWeight));
					builder->CreateCondBr(isBool, *primitiveBoolBlock, _refValueBlock, (boolWeight > refWeight) ? GetLikelyFirstBranchMetadata() : GetLikelySecondBranchMetadata());

					builder->SetInsertPoint(*primitiveBoolBlock);
					*primitiveBoolVar = UnpackBool(builder, value);
					cases += 1;
				}
			}
			*refValueBlock = _refValueBlock;

			builder->SetInsertPoint(origBlock);

			return cases;
		}
		llvm::Value* RefValueHeader::GenerateReadTypeTag(NomBuilder& builder, llvm::Value* refValue)
		{
			return builder->CreateTrunc(builder->CreatePtrToInt(MakeLoad(builder, builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), MakeInt32(RefValueHeaderFields::InterfaceTable)), numtype(intptr_t)), IntegerType::get(LLVMCONTEXT, 3));
		}
		llvm::Value* RefValueHeader::GenerateGetReserveTypeArgsFromVTablePointer(NomBuilder& builder, llvm::Value* vtablePtr)
		{
			auto pointerAsInt = builder->CreatePtrToInt(vtablePtr, numtype(intptr_t));
			return builder->CreateTrunc(builder->CreateLShr(pointerAsInt, MakeIntLike(pointerAsInt, bitsin(intptr_t) / 2)), numtype(int32_t));
		}

		llvm::Value* RefValueHeader::GenerateWriteVTablePointer(NomBuilder& builder, llvm::Value* refValue, llvm::Value* vtableptr)
		{
			return MakeInvariantStore(builder, builder->CreatePointerCast(vtableptr, RTVTable::GetLLVMType()->getPointerTo()), builder->CreateGEP(builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32(RefValueHeaderFields::InterfaceTable) }));
		}
		llvm::Value* RefValueHeader::GenerateWriteVTablePointerCMPXCHG(NomBuilder& builder, llvm::Value* refValue, llvm::Value* vtableptr, llvm::Value* vtableValue)
		{
			auto vtableAddress = builder->CreateGEP(builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32(RefValueHeaderFields::InterfaceTable) });
			return builder->CreateAtomicCmpXchg(vtableAddress, builder->CreatePointerCast(vtableValue, RTVTable::GetLLVMType()->getPointerTo()), builder->CreatePointerCast(vtableptr, RTVTable::GetLLVMType()->getPointerTo()), AtomicOrdering::AcquireRelease, AtomicOrdering::Acquire);
		}
		void RefValueHeader::GenerateInitializerCode(NomBuilder& builder, llvm::Value* valueHeader, llvm::Constant* vTablePointer, llvm::Constant* rawInvokePointer)
		{
			GenerateWriteVTablePointer(builder, valueHeader, vTablePointer);
			if (rawInvokePointer != nullptr)
			{
				GenerateWriteRawInvoke(builder, valueHeader, rawInvokePointer);
			}
		}

		llvm::Value* RefValueHeader::GetInterfaceMethodTableFunction(NomBuilder& builder, CompileEnv* env, RegIndex reg, llvm::Constant* index, int lineno)
		{
			Function* fun = builder->GetInsertBlock()->getParent();
			auto recreg = (*env)[reg];

			BasicBlock* refValueBlock = nullptr, * packedIntBlock = nullptr, * packedFloatBlock = nullptr, * primitiveIntBlock = nullptr, * primitiveFloatBlock = nullptr, * primitiveBoolBlock = nullptr;

			BasicBlock* mergeBlock;
			PHINode* tablePHI;

			GenerateRefOrPrimitiveValueSwitch(builder, recreg, &refValueBlock, &packedIntBlock, &packedFloatBlock, false, &primitiveIntBlock, nullptr, &primitiveFloatBlock, nullptr, &primitiveBoolBlock, nullptr);

			int count = (refValueBlock != nullptr ? 1 : 0) + (packedIntBlock != nullptr ? 1 : 0) + (packedFloatBlock != nullptr ? 1 : 0) + (primitiveIntBlock != nullptr ? 1 : 0) + (primitiveFloatBlock != nullptr ? 1 : 0) + (primitiveBoolBlock != nullptr ? 1 : 0);

			if (count == 0)
			{
				throw new std::exception();
			}
			if (count > 1)
			{
				mergeBlock = BasicBlock::Create(LLVMCONTEXT, "imtLookupMerge", fun);

				builder->SetInsertPoint(mergeBlock);
				tablePHI = builder->CreatePHI(GetIMTFunctionType()->getPointerTo(), count, "imt");
			}

			if (refValueBlock != nullptr)
			{
				builder->SetInsertPoint(refValueBlock);
				Value* vtable = GenerateReadVTablePointer(builder, recreg);
				auto vte = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, vtable, index);
				if (count == 1)
				{
					return vte;
				}
				else
				{
					tablePHI->addIncoming(vte, builder->GetInsertBlock());
					builder->CreateBr(mergeBlock);
				}
			}
			if (packedIntBlock != nullptr)
			{
				builder->SetInsertPoint(packedIntBlock);
				auto clsConstant = NomIntClass::GetInstance()->GetLLVMElement(*fun->getParent());
				auto vte = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, clsConstant, index);
				if (count == 1)
				{
					return vte;
				}
				else
				{
					tablePHI->addIncoming(vte, builder->GetInsertBlock());
					builder->CreateBr(mergeBlock);
				}
			}
			if (primitiveIntBlock != nullptr)
			{
				builder->SetInsertPoint(primitiveIntBlock);
				auto clsConstant = NomIntClass::GetInstance()->GetLLVMElement(*fun->getParent());
				auto vte = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, clsConstant, index);
				if (count == 1)
				{
					return vte;
				}
				else
				{
					tablePHI->addIncoming(vte, builder->GetInsertBlock());
					builder->CreateBr(mergeBlock);
				}
			}
			if (packedFloatBlock != nullptr)
			{
				builder->SetInsertPoint(packedFloatBlock);
				auto clsConstant = NomFloatClass::GetInstance()->GetLLVMElement(*fun->getParent());
				auto vte = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, clsConstant, index);
				if (count == 1)
				{
					return vte;
				}
				else
				{
					tablePHI->addIncoming(vte, builder->GetInsertBlock());
					builder->CreateBr(mergeBlock);
				}
			}
			if (primitiveFloatBlock != nullptr)
			{
				builder->SetInsertPoint(primitiveFloatBlock);
				auto clsConstant = NomFloatClass::GetInstance()->GetLLVMElement(*fun->getParent());
				auto vte = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, clsConstant, index);
				if (count == 1)
				{
					return vte;
				}
				else
				{
					tablePHI->addIncoming(vte, builder->GetInsertBlock());
					builder->CreateBr(mergeBlock);
				}
			}
			if (primitiveBoolBlock != nullptr)
			{
				builder->SetInsertPoint(primitiveBoolBlock);
				auto clsConstant = NomBoolClass::GetInstance()->GetLLVMElement(*fun->getParent());
				auto vte = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, clsConstant, index);
				if (count == 1)
				{
					return vte;
				}
				else
				{
					tablePHI->addIncoming(vte, builder->GetInsertBlock());
					builder->CreateBr(mergeBlock);
				}
			}
			builder->SetInsertPoint(mergeBlock);
			return tablePHI;
			//BasicBlock* incomingBlock = builder->GetInsertBlock();
			//
			//if (recreg.GetNomType()->PossiblyPrimitive())
			//{
			//	BasicBlock* isInt = BasicBlock::Create(LLVMCONTEXT, "imp_int", env->Function);
			//	BasicBlock* isFloat = BasicBlock::Create(LLVMCONTEXT, "imp_float", env->Function);
			//	BasicBlock* isRef = BasicBlock::Create(LLVMCONTEXT, "imp_rec", env->Function);
			//	incomingBlock = BasicBlock::Create(LLVMCONTEXT, "imp_merge", env->Function);

			//	CreateRefKindSwitch(builder, recreg, isFloat, isRef, isInt);

			//	builder->SetInsertPoint(isInt);
			//	auto intTable = builder->CreatePointerCast(NomIntClass::GetInstance()->GetInterfaceTableLookup(*incomingBlock->getParent()->getParent(), llvm::GlobalValue::LinkageTypes::ExternalLinkage), GetIMTFunctionType()->getPointerTo()); //ConstantExpr::getPointerCast(NomIntClass::GetInstance()->GetLLVMElement(*env->Function->getParent()), RTVTable::GetLLVMType()->getPointerTo());
			//	builder->CreateBr(incomingBlock);

			//	builder->SetInsertPoint(isFloat);
			//	auto floatTable = builder->CreatePointerCast(NomFloatClass::GetInstance()->GetInterfaceTableLookup(*incomingBlock->getParent()->getParent(), llvm::GlobalValue::LinkageTypes::ExternalLinkage), GetIMTFunctionType()->getPointerTo()); //ConstantExpr::getPointerCast(NomFloatClass::GetInstance()->GetLLVMElement(*env->Function->getParent()), RTVTable::GetLLVMType()->getPointerTo());
			//	builder->CreateBr(incomingBlock);

			//	builder->SetInsertPoint(isRef);
			//	Value* vtable = GenerateReadVTablePointer(builder, recreg);
			//	auto refTable = RTVTable::GenerateReadInterfaceMethodTableEntry(builder, vtable, index);
			//	builder->CreateBr(incomingBlock);

			//	builder->SetInsertPoint(incomingBlock);
			//	PHINode* tablePHI = builder->CreatePHI(GetIMTFunctionType()->getPointerTo(), 3, "imt");
			//	tablePHI->addIncoming(intTable, isInt);
			//	tablePHI->addIncoming(floatTable, isFloat);
			//	tablePHI->addIncoming(refTable, isRef);

			//	return tablePHI;

			//}
			//Value* vtable = GenerateReadVTablePointer(builder, recreg);
			//return RTVTable::GenerateReadInterfaceMethodTableEntry(builder, vtable, index);
		}

		llvm::Value* RefValueHeader::GenerateReadRawInvoke(NomBuilder& builder, llvm::Value* refValue)
		{
			if (NomLambdaOptimizationLevel == 0)
			{
				throw new std::exception();
			}
			return MakeInvariantLoad(builder, builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), { MakeInt32(RefValueHeaderFields::RawInvoke),MakeInt32(0) });
		}


		void RefValueHeader::GenerateWriteRawInvoke(NomBuilder& builder, llvm::Value* refValue, llvm::Value* rawInvokePointer)
		{
			if (NomLambdaOptimizationLevel == 0)
			{
				throw new std::exception();
			}
			MakeStore(builder, builder->CreatePointerCast(rawInvokePointer, POINTERTYPE), builder->CreateGEP(builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32((unsigned char)RefValueHeaderFields::RawInvoke),MakeInt<int32_t>(0) }));
		}
		llvm::AtomicCmpXchgInst* RefValueHeader::GenerateWriteRawInvokeCMPXCHG(NomBuilder& builder, llvm::Value* refValue, llvm::Value* previousValueForCMPXCHG, llvm::Value* rawInvokePointer)
		{
			if (NomLambdaOptimizationLevel == 0)
			{
				throw new std::exception();
			}
			return builder->CreateAtomicCmpXchg(builder->CreateGEP(builder->CreatePointerCast(refValue, GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32((unsigned char)RefValueHeaderFields::RawInvoke),MakeInt<int32_t>(0) }),
				previousValueForCMPXCHG,
				builder->CreatePointerCast(rawInvokePointer, POINTERTYPE),
				llvm::AtomicOrdering::Monotonic,
				llvm::AtomicOrdering::Monotonic);
		}

		//llvm::Value* RefValueHeader::GetInterfaceMethodPointer(NomBuilder& builder, CompileEnv* env, RegIndex reg, int lineno, InterfaceID iface, int offset)
		//{
		//	BasicBlock* incomingBlock = builder->GetInsertBlock();
		//	BasicBlock* regularTable = BasicBlock::Create(LLVMCONTEXT, "regularITable", env->Function);
		//	BasicBlock* multiCastList = BasicBlock::Create(LLVMCONTEXT, "multiCastList", env->Function);

		//	//BasicBlock* checkBlock = BasicBlock::Create(LLVMCONTEXT, "", env->Function);
		//	//BasicBlock* loopBlock = BasicBlock::Create(LLVMCONTEXT, "", env->Function);
		//	//BasicBlock* lookupBlock = BasicBlock::Create(LLVMCONTEXT, "", env->Function);
		//	//BasicBlock* afterBlock = BasicBlock::Create(LLVMCONTEXT, "", env->Function);

		//	auto recreg = (*env)[reg];

		//	//load pointer to vtable
		//	Value* vtable;
		//	if (recreg.GetNomType()->PossiblyPrimitive())
		//	{
		//		BasicBlock* isInt = BasicBlock::Create(LLVMCONTEXT, "imp_int", env->Function);
		//		BasicBlock* isFloat = BasicBlock::Create(LLVMCONTEXT, "imp_float", env->Function);
		//		BasicBlock* isRef = BasicBlock::Create(LLVMCONTEXT, "imp_rec", env->Function);
		//		incomingBlock = BasicBlock::Create(LLVMCONTEXT, "imp_merge", env->Function);

		//		//Value * tag = builder->CreateTrunc(builder->CreatePtrToInt(recreg, INTTYPE, "recasint"), numtype(2), "rectag");
		//		CreateRefKindSwitch(builder, recreg, isFloat, isRef, isInt);

		//		builder->SetInsertPoint(isInt);
		//		auto intTable = ConstantExpr::getPointerCast(NomIntClass::GetInstance()->GetLLVMElement(*env->Function->getParent()), RTVTable::GetLLVMType()->getPointerTo());
		//		builder->CreateBr(incomingBlock);

		//		builder->SetInsertPoint(isFloat);
		//		auto floatTable = ConstantExpr::getPointerCast(NomFloatClass::GetInstance()->GetLLVMElement(*env->Function->getParent()), RTVTable::GetLLVMType()->getPointerTo());
		//		builder->CreateBr(incomingBlock);

		//		builder->SetInsertPoint(isRef);
		//		auto refTable = GenerateReadVTablePointer(builder, recreg);
		//		builder->CreateBr(incomingBlock);

		//		builder->SetInsertPoint(incomingBlock);
		//		auto vtablephi = builder->CreatePHI(RTVTable::GetLLVMType()->getPointerTo(), 3, "vtable");
		//		vtablephi->addIncoming(intTable, isInt);
		//		vtablephi->addIncoming(floatTable, isFloat);
		//		vtablephi->addIncoming(refTable, isRef);
		//		vtable = vtablephi;
		//	}
		//	else
		//	{

		//		vtable = GenerateReadVTablePointer(builder, recreg); 
		//	}

		//	//Value* mtablesizeaddr = builder->CreateGEP(clsdescptr_typed, { MakeInt32(0), MakeInt32((unsigned char)RTClassFields::InterfaceTableSize) });
		//	auto ifaceTableOffset = RTVTable::GenerateReadInterfaceTableOffset(builder, vtable);
		//	auto flag = builder->CreateTrunc(ifaceTableOffset, inttype(1), "itableflag");
		//	builder->CreateIntrinsic(Intrinsic::expect, { inttype(1) }, { flag, MakeUInt(1, 1) });
		//	builder->CreateCondBr(flag, regularTable, multiCastList);

		//	//builder->SetInsertPoint(regularTable);
		//	//auto indexPHI = builder->CreatePHI(inttype(32), 2, "itableIndex");
		//	//indexPHI->addIncoming(MakeInt32(0), incomingBlock);
		//	//indexPHI->addIncoming(builder->CreateAdd(indexPHI, MakeInt32(1)), regularTable);
		//	//Value* mtablesize = RTVTable::GenerateReadNumInterfaceTableEntries(builder, vtable); //RTClass::GenerateReadInterfaceTableSize(builder, *(env->Module), clsdescptr_typed);  //MakeLoad(builder, *(env->Module), mtablesizeaddr);
		//	//Value* mtableptraddr = builder->CreateGEP(clsdescptr_typed, { MakeInt32(0), MakeInt32((unsigned char)RTClassFields::InterfaceTable) }); //load pointer to method table
		//	//Value* entry = RTVTable::GetInterfaceTableEntryPointer(builder, vtable, ifaceTableOffset, MakeInt<InterfaceID>(iface));
		//	//Value* entryKey = RTInterfaceTableEntry::GenerateReadKey(builder, entry);
		//	//auto keyMatch = builder->CreateICmpEQ(builder->CreatePtrToInt(entryKey, numtype(InterfaceID)), MakeInt<InterfaceID>(iface), "match");
		//	//builder->CreateCondBr(keyMatch, afterBlock, regularTable);
		//	//builder->CreateBr(afterBlock);

		//	//Value* mtableptr = RTVTable::GenerateReadFirstInterfaceTableEntryPointer(builder, vtable);  //RTClass::GenerateReadInterfaceTable(builder, *(env->Module), clsdescptr_typed); //MakeLoad(builder, *(env->Module), mtableptraddr);

		//	builder->SetInsertPoint(multiCastList);
		//	static const char* notimplemented_errorMessage = "Multicast interface dispatch not implemented!";
		//	builder->CreateCall(RTOutput_Fail::GetLLVMElement(*env->Module), GetLLVMPointer(notimplemented_errorMessage))->setCallingConv(RTOutput_Fail::GetLLVMElement(*env->Module)->getCallingConv());
		//	CreateDummyReturn(builder, env->Function);


		//	builder->SetInsertPoint(regularTable);
		//	auto address = builder->CreateSub(ifaceTableOffset, MakeInt<intptr_t>(1), "lookup-Address");
		//	return builder->CreateIntToPtr(address, GetIMTFunctionType()->getPointerTo());

		//	//auto ifacelookupfun = builder->CreateIntToPtr(address, NomClass::GetInterfaceTableLookupType()->getPointerTo());
		//	//auto ifaceoffset = builder->CreateCall(NomClass::GetInterfaceTableLookupType(), ifacelookupfun, {/*vtable,*/ MakeInt<InterfaceID>(iface)/*, MakeInt<int32_t>(offset)*/}, "ifaceoffset");
		//	//ifaceoffset->setCallingConv(NOMCC);
		//	////return ifaceoffset;//actually a method pointer now
		//	//return RTVTable::GenerateReadMethodTableEntry(builder, vtable, builder->CreateAdd(ifaceoffset, MakeInt32(-1 - offset)));

		//	//builder->CreateBr(checkBlock);
		//	//builder->SetInsertPoint(checkBlock);
		//	//llvm::PHINode* phi = builder->CreatePHI(numtype(size_t), 2);
		//	//phi->addIncoming(zero, incomingBlock);
		//	//phi->addIncoming(builder->CreateAdd(phi, MakeInt<size_t>(1)), loopBlock);

		//	//builder->CreateCondBr(builder->CreateICmpSLT(phi, mtablesize), loopBlock, afterBlock);

		//	//builder->SetInsertPoint(loopBlock);
		//	//Value* ikeyaddr = builder->CreateGEP(mtableptr, { {phi, MakeInt32(0)} });
		//	//Value* ikey = MakeLoad(builder, *(env->Module), ikeyaddr);

		//	//builder->CreateCondBr(builder->CreateICmpEQ(ikey, MakeInt<InterfaceID>(iface)), lookupBlock, checkBlock);

		//	//builder->SetInsertPoint(lookupBlock);

		//	//Value* ifaceTableAddr = builder->CreateGEP(mtableptr, { {phi, MakeInt32(1)} });
		//	//Value* ifaceTable = MakeLoad(builder, *(env->Module), ifaceTableAddr);

		//	//Value* resultAddr = builder->CreateGEP(ifaceTable, { { MakeInt32(0), MakeInt32(offset)} });
		//	//Value* result = MakeLoad(builder, *(env->Module), resultAddr);

		//	//builder->CreateBr(afterBlock);

		//	//builder->SetInsertPoint(afterBlock);
		//	//PHINode* phi2 = builder->CreatePHI(POINTERTYPE, 2);
		//	//phi2->addIncoming(ConstantPointerNull::get(POINTERTYPE), checkBlock);
		//	//phi2->addIncoming(result, lookupBlock);
		//	//return phi2;
		//	//return RTVTable::GenerateReadMethodTableEntry(builder, vtable, builder->CreateAdd(RTInterfaceTableEntry::GenerateReadMethodOffset(builder, entry), MakeInt32(-1 - offset)));
		//	//return RTVTable::GenerateReadMethodTableEntry(builder, vtable, builder->CreateAdd(RTInterfaceTableEntry::GenerateReadMethodOffset(builder, entry), MakeInt32(-1 - offset)));
		//}
	}
}