#pragma once
#include "Defs.h"
#include "AvailableExternally.h"
#include "llvm/IR/Constants.h"
#include "NomMemberContext.h"
#include "NomField.h"
#include "NomCallable.h"
#include <vector>

namespace Nom
{
	namespace Runtime
	{
		class NomLambda;
		class NomSubstitutionContext;
		class NomLambdaBody : public NomCallableLoaded
		{
		private:
			mutable std::string symname;
			//mutable TypeList argumentTypes = TypeList((NomTypeRef *)nullptr, (size_t)0);
		public:
			NomLambda* const Parent;
			//ConstantID ArgTypes;
			ConstantID ReturnType;
			NomLambdaBody(NomLambda* parent, const RegIndex regcount, ConstantID typeParams, ConstantID argTypes, ConstantID returnType);
			virtual ~NomLambdaBody() override = default;

			// Inherited via NomCallable
			virtual Function* createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const override;
			virtual NomTypeRef GetReturnType(const NomSubstitutionContext *context) const override;
			//virtual TypeList GetArgumentTypes(const NomSubstitutionContext* context) const override;
			//virtual int GetArgumentCount() const override;
			virtual llvm::FunctionType* GetLLVMFunctionType(const NomSubstitutionContext* context = nullptr) const override;

			// Inherited via NomCallable
			virtual const llvm::ArrayRef<NomTypeParameterRef> GetDirectTypeParameters() const override;
			virtual size_t GetDirectTypeParametersCount() const override;
			virtual NomTypeParameterRef GetLocalTypeParameter(int index) const override;
			virtual const NomMemberContext* GetParent() const override;
			virtual const std::string& GetName() const override;
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override;
			virtual const std::string* GetSymbolName() const override;

			virtual void PushDependencies(std::set<ConstantID>& set) const override
			{
				NomCallableLoaded::PushDependencies(set);
				set.insert(ReturnType);
			}
		};
		class NomLambda : public NomCallableLoaded
		{
		private:
			static std::vector<NomLambda*>& preprocessQueue();
			mutable std::string symname;
			std::vector<NomField*> Fields;
			//mutable std::vector<NomTypeRef> argTypes;
			//const ConstantID argTypesID;
			mutable bool preprocessed = false;
		public:
			const ConstantID ID;
			NomLambdaBody Body;
			NomLambda(ConstantID id, const NomMemberContext* parent, const RegIndex regcount, ConstantID closureTypeParams, ConstantID closureArguments, ConstantID typeParams, ConstantID argTypes, ConstantID returnType);
			virtual ~NomLambda() override = default;

			static void ProcessPreprocessQueue();

			NomClosureField* AddField(const ConstantID name, const ConstantID type);
			void PreprocessInheritance() const;
			const NomField* GetField(NomStringRef name) const;


			// Inherited via AvailableExternally
			virtual llvm::Function* createLLVMElement(llvm::Module& mod, llvm::GlobalValue::LinkageTypes linkage) const override;
			virtual llvm::Function* findLLVMElement(llvm::Module& mod) const override;

			virtual NomTypeRef GetReturnType(const NomSubstitutionContext* context) const override;
			//virtual TypeList GetArgumentTypes(const NomSubstitutionContext* context) const override;
			//virtual int GetArgumentCount() const override;

			// Inherited via NomMemberContext
			virtual const std::string* GetSymbolName() const override;

			// Inherited via NomCallableLoaded
			virtual llvm::FunctionType* GetLLVMFunctionType(const NomSubstitutionContext* context = nullptr) const override;
			virtual llvm::ArrayRef<NomTypeParameterRef> GetArgumentTypeParameters() const override;

			virtual void PushDependencies(std::set<ConstantID>& set) const override
			{
				NomCallableLoaded::PushDependencies(set);
				Body.PushDependencies(set);
				for (auto nf : Fields)
				{
					nf->PushDependencies(set);
				}
			}
		};
	}
}