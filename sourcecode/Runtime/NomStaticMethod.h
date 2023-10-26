#pragma once

#include "NomInstruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "NomType.h"
#include "TypeList.h"
#include "Defs.h"
#include <vector>
#include <string>
#include "NomCallable.h"
#include "NomMemberContext.h"

namespace Nom
{
	namespace Runtime
	{
		class NomStaticMethod : public virtual NomCallable
		{
		protected:
			NomStaticMethod()
			{}
		public:
			virtual ~NomStaticMethod()
			{}

			virtual llvm::FunctionType* GetLLVMFunctionType(const NomSubstitutionContext* context = nullptr) const override;

			virtual const NomClass* GetContainer() const = 0;
		};

		class NomStaticMethodLoaded : public virtual NomStaticMethod, public NomCallableLoaded
		{
		private:
			const ConstantID returnType;
			mutable NomTypeRef returnTypeBuf;
		public:
			const NomClass* const Class;
			NomStaticMethodLoaded(const std::string& name, const NomClass* parent, const std::string& qname, const ConstantID returnType, const ConstantID typeArgs, const ConstantID arguments, const int regcount, bool declOnly = false);
			virtual ~NomStaticMethodLoaded() override = default;


			virtual llvm::Function * createLLVMElement(llvm::Module &mod, llvm::GlobalValue::LinkageTypes linkage) const override;

			virtual NomTypeRef GetReturnType(const NomSubstitutionContext* context) const override;
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override;

			virtual const NomClass* GetContainer() const override;

			virtual void PushDependencies(std::set<ConstantID>& set) const override
			{
				NomCallableLoaded::PushDependencies(set);
				set.insert(returnType);
			}
		};


		class NomStaticMethodInternal : public virtual NomStaticMethod, public NomCallableInternal
		{
		private:
			NomTypeRef returnType = nullptr;
		public:
			const NomClass* const Container;
			NomStaticMethodInternal(const std::string& name, const std::string& qname, const NomClass* cls);
			virtual ~NomStaticMethodInternal() override;

			virtual llvm::Function* createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const override;

			virtual NomTypeRef GetReturnType(const NomSubstitutionContext* context) const override;
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override;
			virtual const NomClass* GetContainer() const override;


			void SetReturnType(NomTypeRef returnType);
		};

	}
}
