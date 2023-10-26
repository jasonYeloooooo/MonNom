#pragma once
#include "NomCallable.h"
#include "GloballyNamed.h"
#include <vector>
#include "llvm/IR/Function.h"

namespace Nom
{
	namespace Runtime
	{
		class NomClass;

		class NomInstance: public virtual NomCallable, public GloballyNamed
		{
		protected:
			NomInstance();
		public:
			virtual ~NomInstance() override = default;

			virtual const NomClass* GetClass() const = 0;

			virtual llvm::FunctionType* GetLLVMFunctionType(const NomSubstitutionContext* context = nullptr) const override;
		};


		class NomInstanceLoaded : public NomInstance, public NomCallableLoaded
		{
		private:
			mutable NomTypeRef returnTypeBuf;
			std::vector<NomInstruction*> Instructions;
			std::vector<RegIndex> superInstanceArgs;
		public:
			const NomClass* const Class;
			NomInstanceLoaded(const NomClass* cls, const std::string& name, const std::string& qname, const ConstantID arguments, const RegIndex regcount, const ConstantID typeArgs, bool declOnly = false, bool cppWrapper = false);
			virtual ~NomInstanceLoaded() override;

			void AddInstruction(NomInstruction* instr)
			{
				Instructions.push_back(instr);
				if (instr->GetOpCode() == OpCode::PhiNode)
				{
					phiNodes.push_back((PhiNode*)instr);
				}
			}
			void AddSuperInstanceArgReg(RegIndex reg)
			{
				superInstanceArgs.push_back(reg);
			}
			// Inherited via NomCallable
			virtual llvm::Function* createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const override;

			// Inherited via NomCallable
			virtual NomTypeRef GetReturnType(const NomSubstitutionContext* context = nullptr) const override;

			virtual const NomClass* GetClass() const override
			{
				return Class;
			}

			// Inherited via NomConstructor
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override;

			virtual void PushDependencies(std::set<ConstantID>& set) const override
			{
				NomCallableLoaded::PushDependencies(set);
				NOM_CONSTANT_DEPENCENCY_CONTAINER depBuf;
				for (auto inst : Instructions)
				{
					depBuf.clear();
					inst->FillConstantDependencies(depBuf);
					for (auto cid : depBuf)
					{
						set.insert(cid);
					}
				}
			}
		};

		class NomInstanceInternal : public NomInstance, public NomCallableInternal
		{
		public:
			const NomClass* const Container;
			NomInstanceInternal(const std::string& qname, const NomClass* cls);
			virtual ~NomInstanceInternal() override;

			// Inherited via NomCallable
			virtual llvm::Function* createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const override;

			// Inherited via NomConstructor
			virtual NomTypeRef GetReturnType(const NomSubstitutionContext* context) const override;
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override;
			virtual const NomClass* GetClass() const override;
		};

	}
}

