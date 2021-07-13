#include "RTStruct.h"
#include "Context.h"
#include "RTVTable.h"
#include "NomInterface.h"
#include "CompileHelpers.h"
#include "NomStruct.h"

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{
		llvm::StructType* RTStruct::GetLLVMType()
		{
			static auto rtst = StructType::create(LLVMCONTEXT, "RT_NOM_StructDescriptor");
			static bool once = true;
			if (once)
			{
				once = false;
				rtst->setBody(
					{ POINTERTYPE,								// NomIR link
					POINTERTYPE,								//field lookup
					POINTERTYPE,								//field store
					POINTERTYPE,								//dispatcher lookup
					//POINTERTYPE,								// shared dictionary
					numtype(size_t),							// field count
					numtype(size_t),							// typearg count
					numtype(size_t),							// number of preallocated type argument slots
					numtype(uint64_t),							// cast ID for specialized vtable
					RTVTable::GetLLVMType()->getPointerTo()		// specialized vtable
					});
			}
			return rtst;
		}

		llvm::Constant* RTStruct::CreateConstant(const NomStruct *structptr, llvm::Function* fieldRead, llvm::Function* fieldWrite, llvm::Function* lookupDispatcher/*, llvm::Constant* dictPointer*/)
		{
			return ConstantStruct::get(GetLLVMType(), GetLLVMPointer(structptr), ConstantExpr::getPointerCast(fieldRead, POINTERTYPE), ConstantExpr::getPointerCast(fieldWrite, POINTERTYPE), ConstantExpr::getPointerCast(lookupDispatcher, POINTERTYPE), /*dictPointer,*/ MakeInt<size_t>(structptr->Fields.size()), MakeInt<size_t>(structptr->GetTypeParametersCount()), MakeInt<size_t>(0), MakeInt<uint64_t>(0), ConstantPointerNull::get(RTVTable::GetLLVMType()->getPointerTo()));
		}

		llvm::Value* RTStruct::GenerateReadPreallocatedSlots(NomBuilder& builder, llvm::Value* descriptor)
		{
			return MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::PreallocatedSlots), "preAllocSlots");
		}
		llvm::Value* RTStruct::GenerateReadFieldLookup(NomBuilder& builder, llvm::Value* descriptor)
		{
			return builder->CreatePointerCast(MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::FieldLookup), "fieldLookup"), NomStruct::GetDynamicFieldLookupType()->getPointerTo());
		}
		llvm::Value* RTStruct::GenerateReadFieldStore(NomBuilder& builder, llvm::Value* descriptor)
		{
			return builder->CreatePointerCast(MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::FieldStore), "fieldStore"), NomStruct::GetDynamicFieldStoreType()->getPointerTo());
		}
		llvm::Value* RTStruct::GenerateReadDispatcherLookup(NomBuilder& builder, llvm::Value* descriptor)
		{
			return builder->CreatePointerCast(MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::DispatcherLookup), "dispatcherLookup"), NomStruct::GetDynamicDispatcherLookupType()->getPointerTo());
		}
		llvm::Value* RTStruct::GenerateReadFieldCount(NomBuilder& builder, llvm::Value* descriptor)
		{
			return MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::FieldCount), "fieldCount");
		}
		llvm::Value* RTStruct::GenerateReadTypeArgCount(NomBuilder& builder, llvm::Value* descriptor)
		{
			return MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::TypeArgCount), "typeArgCount");
		}
		llvm::Value* RTStruct::GenerateReadSpecializedVTableCastID(NomBuilder& builder, llvm::Value* descriptor)
		{
			return MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::SpecializedVTableCastID), "SpecializedVTableCastID");
		}
		llvm::Value* RTStruct::GenerateReadSpecializedVTable(NomBuilder& builder, llvm::Value* descriptor)
		{
			return MakeLoad(builder, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::SpecializedVTable), "specializedVtable");
		}
		void RTStruct::GenerateWriteSpecializedVTable(NomBuilder& builder, llvm::Value* descriptor, llvm::Value* castID, llvm::Value* vtable)
		{
			auto address = builder->CreateGEP(builder->CreatePointerCast(descriptor, RTStruct::GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32(RTStructFields::SpecializedVTable) });
			auto cmpx = builder->CreateAtomicCmpXchg(address, ConstantPointerNull::get(RTVTable::GetLLVMType()->getPointerTo()), builder->CreatePointerCast(vtable, RTVTable::GetLLVMType()->getPointerTo()), AtomicOrdering::Monotonic, AtomicOrdering::Monotonic);

			if (castID != nullptr)
			{
				BasicBlock* successBlock = BasicBlock::Create(LLVMCONTEXT, "cmpxSuccess", builder->GetInsertBlock()->getParent());
				BasicBlock* outBlock = BasicBlock::Create(LLVMCONTEXT, "cmpxFail_continue", builder->GetInsertBlock()->getParent());
				builder->CreateCondBr(builder->CreateExtractValue(cmpx, { 1 }, "cmpxIsSuccess"), successBlock, outBlock);

				builder->SetInsertPoint(successBlock);
				MakeStore(builder, castID, descriptor, RTStruct::GetLLVMType()->getPointerTo(), MakeInt32(RTStructFields::SpecializedVTableCastID));
				builder->CreateBr(outBlock);

				builder->SetInsertPoint(outBlock);
			}
		}
	}
}