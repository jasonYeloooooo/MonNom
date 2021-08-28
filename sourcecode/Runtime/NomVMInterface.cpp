#include "NomVMInterface.h"
#include "llvm/IR/Function.h"
#include "Context.h"
#include "Defs.h"
#include "NomConstants.h"
#include "llvm/IR/DerivedTypes.h"
#include "NomString.h"
#include "llvm/Support/DynamicLibrary.h"
#include <iostream>
#include <forward_list>
#include <cinttypes>
#include "NullClass.h"
#include "NomAlloc.h"
#include "LambdaHeader.h"
#include "RTVTable.h"
#include "RTCast.h"
#include "StructHeader.h"
#include "gcinclude_config.h"
#include "CastStats.h"
#include "RTSubtyping.h"

using namespace llvm;
using namespace Nom::Runtime;

llvm::Function* GetPrint(llvm::Module* mod)
{
	Function* ret = mod->getFunction("CPP_NOM_Print");
	if (ret == nullptr)
	{
		std::array<Type*, 1> chararrpluslen = { { Type::getIntNTy(LLVMCONTEXT, bitsin(uint64_t))} };
		FunctionType* printFunType = FunctionType::get(Type::getVoidTy(LLVMCONTEXT), chararrpluslen, false);
		ret = Function::Create(printFunType, Function::ExternalLinkage, "CPP_NOM_Print", mod);
	}
	return ret;
}

llvm::Function* GetAlloc(llvm::Module* mod)
{
	Function* ret = mod->getFunction("CPP_NOM_GCALLOC");
	if (ret == nullptr)
	{
		FunctionType* allocFunType = FunctionType::get(POINTERTYPE, { INTTYPE }, false);
		ret = Function::Create(allocFunType, Function::ExternalLinkage, "CPP_NOM_GCALLOC", mod);
	}
	return ret;
}

llvm::Function* GetNewAlloc(llvm::Module* mod)
{
	Function* ret = mod->getFunction("CPP_NOM_NEWALLOC");
	if (ret == nullptr)
	{
		FunctionType* allocFunType = FunctionType::get(REFTYPE, { INTTYPE, INTTYPE }, false);
		ret = Function::Create(allocFunType, Function::ExternalLinkage, "CPP_NOM_NEWALLOC", mod);
	}
	return ret;
}

llvm::Function* GetClosureAlloc(llvm::Module* mod)
{
	Function* ret = mod->getFunction("CPP_NOM_CLOSUREALLOC");
	if (ret == nullptr)
	{
		FunctionType* allocFunType = FunctionType::get(LambdaHeader::GetLLVMType()->getPointerTo(), { INTTYPE, INTTYPE, INTTYPE }, false);
		ret = Function::Create(allocFunType, Function::ExternalLinkage, "CPP_NOM_CLOSUREALLOC", mod);
	}
	return ret;
}

llvm::Function* GetStructAlloc(llvm::Module* mod)
{
	Function* ret = mod->getFunction("CPP_NOM_STRUCTALLOC");
	if (ret == nullptr)
	{
		FunctionType* allocFunType = FunctionType::get(StructHeader::GetLLVMType()->getPointerTo(), { INTTYPE, INTTYPE, INTTYPE }, false);
		ret = Function::Create(allocFunType, Function::ExternalLinkage, "CPP_NOM_STRUCTALLOC", mod);
	}
	return ret;
}

llvm::Function* GetClassTypeAlloc(llvm::Module* mod)
{
	Function* ret = mod->getFunction("CPP_NOM_STRUCTALLOC");
	if (ret == nullptr)
	{
		FunctionType* allocFunType = FunctionType::get(RTClassType::GetLLVMType()->getPointerTo(), { INTTYPE }, false);
		ret = Function::Create(allocFunType, Function::ExternalLinkage, "CPP_NOM_CLASSTYPEALLOC", mod);
	}
	return ret;
}

void GenerateLLVMDebugPrint(IRBuilder<>& builder, llvm::Module* mod, const std::string& str)
{
	static std::forward_list<std::string> strings;
	strings.push_front(str);
	uint64_t strptr = reinterpret_cast<uint64_t>(&(strings.front()));
	fprintf(stdout, "%" PRIu64 "\n", strptr);
	std::cout.flush();
	std::cout << *(reinterpret_cast<std::string*>(strptr));
	llvm::ConstantInt* pointerIntConstant = llvm::ConstantInt::get(Type::getIntNTy(LLVMCONTEXT, bitsin(uint64_t)), strptr, false);
	fprintf(stdout, "%" PRIu64 "\n", pointerIntConstant->getZExtValue());
	std::array<Value*, 1> args = { { pointerIntConstant } };
	builder.CreateCall(GetPrint(mod), args);
}

DLLEXPORT void InitVMInterface()
{
}

extern "C" DLLEXPORT void* CPP_NOM_GCALLOC(size_t size)
{
	if (NomCastStats > 0)
	{
		RT_NOM_STATS_IncAllocations(AllocationType::General);
	}
	auto ret = gcalloc(size);
	return ret;
}

extern "C" DLLEXPORT void* CPP_NOM_NEWALLOC(size_t numfields, size_t numtargs)
{
	if (NomCastStats > 0)
	{
		RT_NOM_STATS_IncAllocations(AllocationType::Object);
	}
	auto ret = (void*)(((char**)gcalloc(ObjectHeader::SizeOf() + (sizeof(char*) * (numfields + numtargs)))) + numtargs);
	return ret;
}

extern "C" DLLEXPORT void* CPP_NOM_CLOSUREALLOC(size_t numfields, size_t numtargs, size_t numreservedargs)
{
	if (NomCastStats > 0)
	{
		RT_NOM_STATS_IncAllocations(AllocationType::Lambda);
	}
	auto ret = (void*)(((char**)gcalloc(LambdaHeader::SizeOf() + (sizeof(char*) * (numfields + numtargs + numreservedargs)))) + /*numreservedargs*/ numtargs);
	return ret;
}

extern "C" DLLEXPORT void* CPP_NOM_STRUCTALLOC(size_t numfields, size_t numtargs, size_t numreservedargs)
{
	if (NomCastStats > 0)
	{
		RT_NOM_STATS_IncAllocations(AllocationType::Struct);
	}
	auto ret = (void*)(((char**)gcalloc(StructHeader::SizeOf() + (sizeof(char*) * (numfields + numtargs + numreservedargs)))) + /*numreservedargs*/ + numtargs);
	auto retDictRoot = (void*)(((char*)(((intptr_t)ret) /*+ numreservedargs*/)) + StructHeader::GetLLVMLayout()->getElementOffset((unsigned int)StructHeaderFields::InstanceDictionary));
	RT_NOM_ConcurrentDictionaryEmplace(retDictRoot);
	return ret;
}

extern "C" DLLEXPORT void* CPP_NOM_CLASSTYPEALLOC(size_t numtargs)
{
	if (NomCastStats > 0)
	{
		RT_NOM_STATS_IncAllocations(AllocationType::ClassType);
	}
	auto ret = (void*)(((char**)gcalloc(RTClassType::SizeOf() + (sizeof(char*) * (numtargs)))) + numtargs);
	return ret;
}

extern "C" DLLEXPORT void CPP_NOM_Print(uint64_t str)
{
	fprintf(stdout, "%" PRIu64 "\n", str);
	std::cout << "PRINT: ";
	std::cout.flush();
	std::cout << *(reinterpret_cast<std::string*>(str));
}
