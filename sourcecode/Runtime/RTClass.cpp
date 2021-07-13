#include "RTClass.h"
#include "ObjectHeader.h"
#include "RTInterfaceTableEntry.h"
#include "NomJIT.h"
#include "NomClass.h"
#include "RTDictionary.h"
#include "CompileHelpers.h"
#include "RTVTable.h"
#include "RTInterface.h"

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{
		llvm::StructType* RTClass::GetLLVMType()
		{
			static StructType* rtct = StructType::create(LLVMCONTEXT, "RT_NOM_ClassDescriptorType");
			static bool once = true;
			if (once)
			{
				once = false;
				rtct->setBody({ RTInterface::GetLLVMType(),		//interface table, flags, nom IR link, type argument count, supertypes
					POINTERTYPE,								//field lookup
					POINTERTYPE,								//field store
					POINTERTYPE,								//dispatcher lookup
					numtype(size_t)								//field count
					});
			}
			return rtct;
		}

		llvm::StructType* RTClass::GetConstantType(llvm::Type * interfaceTableType, llvm::Type * methodTableType)
		{
			return StructType::get(LLVMCONTEXT, { methodTableType, GetLLVMType() }, true);
		}

		llvm::Constant* RTClass::CreateConstant(llvm::GlobalVariable *gvar, llvm::StructType *gvartype, const NomInterface *irptr, llvm::Function* fieldRead, llvm::Function* fieldWrite, llvm::Function* lookupDispatcher, llvm::ConstantInt* fieldCount, llvm::ConstantInt* typeArgCount, llvm::ConstantInt* superTypesCount, llvm::Constant* superTypeEntries, llvm::Constant* methodTable, llvm::Constant* interfaceTable)
		{
			gvar->setInitializer(llvm::ConstantStruct::get(gvartype, methodTable, llvm::ConstantStruct::get(GetLLVMType(), RTInterface::CreateConstant(irptr, RTInterfaceFlags::None, typeArgCount, superTypesCount, ConstantExpr::getGetElementPtr(((PointerType*)superTypeEntries->getType())->getElementType(), superTypeEntries, ArrayRef<Constant*>({ MakeInt32(0), MakeInt32(0) })), interfaceTable, GetLLVMPointer(&irptr->runtimeInstantiations) ), ConstantExpr::getPointerCast(fieldRead,POINTERTYPE), ConstantExpr::getPointerCast(fieldWrite, POINTERTYPE), ConstantExpr::getPointerCast(lookupDispatcher, POINTERTYPE), fieldCount)));
			return ConstantExpr::getGetElementPtr(gvartype, gvar, ArrayRef<llvm::Constant*>({ MakeInt32(0), MakeInt32(1) }));
		}
		llvm::Constant* RTClass::FindConstant(llvm::Module& mod, const StringRef name)
		{
			auto cnst = mod.getGlobalVariable(name);
			if (cnst == nullptr)
			{
				return nullptr;
			}
			return ConstantExpr::getGetElementPtr(cnst->getValueType(), cnst, ArrayRef<llvm::Constant*>({ MakeInt32(0), MakeInt32(1) }));
		}

		llvm::Value* RTClass::GenerateReadKind(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return RTInterface::GenerateReadKind(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTInterface) }));
		}

		llvm::Value* RTClass::GenerateReadNomIRLink(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return RTInterface::GenerateReadNomIRLink(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTInterface) }));
		}

		llvm::Value* RTClass::GenerateReadFieldLookup(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return builder->CreatePointerCast(MakeLoad(builder, builder->CreatePointerCast(descriptorPtr, GetLLVMPointerType()), MakeInt32(RTClassFields::FieldLookup)), NomClass::GetDynamicFieldLookupType()->getPointerTo());
		}

		llvm::Value* RTClass::GenerateReadFieldStore(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return builder->CreatePointerCast(MakeLoad(builder, builder->CreatePointerCast(descriptorPtr, GetLLVMPointerType()), MakeInt32(RTClassFields::FieldStore)), NomClass::GetDynamicFieldStoreType()->getPointerTo());
		}

		llvm::Value* RTClass::GenerateReadDispatcherLookup(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return builder->CreatePointerCast(MakeLoad(builder, builder->CreatePointerCast(descriptorPtr, GetLLVMPointerType()), MakeInt32(RTClassFields::DispatcherLookup)), NomClass::GetDynamicDispatcherLookupType()->getPointerTo());
		}


		llvm::Value* RTClass::GenerateReadFieldCount(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return MakeLoad(builder, builder->CreatePointerCast(descriptorPtr, GetLLVMType()->getPointerTo()), MakeInt32(RTClassFields::FieldCount));
		}
		llvm::Value* RTClass::GenerateReadSuperInstanceCount(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return RTInterface::GenerateReadSuperInstanceCount(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTInterface) }));
			//MakeLoad(builder, builder->CreatePointerCast(descriptorPtr, GetLLVMType()->getPointerTo()), MakeInt32(RTClassFields::SuperTypesCount));
		}
		llvm::Value* RTClass::GenerateReadSuperInstances(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return RTInterface::GenerateReadSuperInstances(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTInterface) }));
			//return MakeLoad(builder, builder->CreatePointerCast(descriptorPtr, GetLLVMType()->getPointerTo()), MakeInt32(RTClassFields::SuperTypes));
		}
		llvm::Value* RTClass::GenerateReadMethodTableEntry(NomBuilder& builder, llvm::Value* vtablePtr, llvm::Value* offset)
		{
			return RTInterface::GenerateReadMethodTableEntry(builder, builder->CreateGEP(builder->CreatePointerCast(vtablePtr, GetLLVMPointerType()), { MakeInt32(0), MakeInt32(RTClassFields::RTInterface) }), offset);
		}
		llvm::Value* RTClass::GenerateReadTypeArgCount(NomBuilder& builder, llvm::Value* descriptorPtr)
		{
			return RTInterface::GenerateReadTypeArgCount(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTInterface) }));
			//return MakeLoad(builder, builder->CreateGEP(builder->CreatePointerCast(descriptorPtr, GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32(RTClassFields::TypeArgCount) }));;
		}
		void RTClass::GenerateInitialization(NomBuilder& builder, llvm::Value* clsptr, llvm::Value* /*vt_ifcoffset*/ vt_imtptr, llvm::Value* vt_kind, llvm::Value* vt_irdesc, llvm::Value* ifc_flags, llvm::Value* ifc_targcount, llvm::Value* ifc_supercount, llvm::Value* ifc_superentries, llvm::Value* fieldlookup, llvm::Value* fieldstore, llvm::Value* displookup, llvm::Value* fieldcount)
		{
			RTInterface::GenerateInitialization(builder, clsptr, /*vt_ifcoffset*/ vt_imtptr, vt_kind, vt_irdesc, ifc_flags, ifc_targcount, ifc_supercount, ifc_superentries);
			llvm::Value* selfptr = builder->CreatePointerCast(clsptr, GetLLVMType()->getPointerTo());
			MakeStore(builder, fieldlookup, selfptr, MakeInt32(RTClassFields::FieldLookup));
			MakeStore(builder, fieldstore, selfptr, MakeInt32(RTClassFields::FieldStore));
			MakeStore(builder, displookup, selfptr, MakeInt32(RTClassFields::DispatcherLookup));
			MakeStore(builder, fieldcount, selfptr, MakeInt32(RTClassFields::FieldCount));
		}
		//llvm::Value* RTClass::GenerateReadNumInterfaceTableEntries(NomBuilder& builder, llvm::Value* descriptorPtr)
		//{
		//	return RTVTable::GenerateReadNumInterfaceTableEntries(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTVTable) }));
		//}
		//llvm::Value* RTClass::GenerateReadFirstInterfaceTableEntryPointer(NomBuilder& builder, llvm::Value* descriptorPtr)
		//{
		//	return RTVTable::GenerateReadFirstInterfaceTableEntryPointer(builder, builder->CreateGEP(descriptorPtr, { MakeInt32(0), MakeInt32(RTClassFields::RTVTable) }));
		//}

		//llvm::Value* RTClass::GenerateReadMethodTable(NomBuilder& builder, llvm::Value* descriptorPtr, llvm::Value* offset)
		//{
		//	if (offset == nullptr)
		//	{
		//		offset = ConstantInt::get(numtype(int32_t), 0);
		//	}
		//	return MakeLoad(builder, builder->CreateGEP(builder->CreatePointerCast(descriptorPtr, GetLLVMType()->getPointerTo()), { MakeInt32(0), MakeInt32(RTClassFields::MethodTable), offset }));;
		//}

		

		//static const llvm::StructLayout *GetLLVMLayout();

		/*uint64_t RTClass::NameOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::NomNamedLink); return offset;
		}
		uint64_t RTClass::MethodTableOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::MethodTable); return offset;
		}
		uint64_t RTClass::ArgCountOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::ArgCount); return offset;
		}
		uint64_t RTClass::FieldCountOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::FieldCount); return offset;
		}
		uint64_t RTClass::SuperTypesCountOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::SuperTypesCount); return offset;
		}
		uint64_t RTClass::SuperTypesOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::SuperTypes); return offset;
		}
		uint64_t RTClass::InterfaceTableSizeOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::InterfaceTableSize); return offset;
		}
		uint64_t RTClass::InterfaceTableOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::InterfaceTable); return offset;
		}
		uint64_t RTClass::DispatchDictOffset()
		{
			static const uint64_t offset = GetLLVMLayout()->getElementOffset((unsigned char)RTClassFields::Dictionary); return offset;
		}*/
		//const char * RTClass::Entry() const
		//{
		//	static llvm::ArrayType * arrtype = llvm::ArrayType::get(GetLLVMType(), 0);
		//	return entry + NomJIT::Instance().getDataLayout().getIndexedOffsetInType(arrtype, llvm::ArrayRef<llvm::Value *>(llvm::ConstantInt::get(INTTYPE, offset, false)));
		//}
		//inline const llvm::StructLayout * RTClass::GetLLVMLayout()
		//{
		//	static const llvm::StructLayout *layout = NomJIT::Instance().getDataLayout().getStructLayout(GetLLVMType()); return layout;
		//}
		//ObjectHeader RTClass::Name() const
		//{
		//	return (ObjectHeader(Entry(NameOffset())));
		//}



		llvm::Value* RTClass::CreateCheckIsExpando(NomBuilder& builder, llvm::Module& mod, llvm::Value* cdesc)
		{
			return ConstantInt::get(BOOLTYPE, 0);
		}
		//size_t RTClass::getSize() const
		//{
		//	return RTClass::SizeOf() + NomJIT::Instance().getDataLayout().getIndexedOffsetInType(REFTYPE, { llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(LLVMCONTEXT), FieldCount()) });
		//}
		//RTClass::RTClass(const char * entry, intptr_t offset, ObjectHeader namestr, void ** methodTable) : entry(entry), offset(offset)
		//{
		//	
		//}
	}
}
