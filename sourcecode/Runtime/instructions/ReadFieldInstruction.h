#pragma once
#include "../NomValueInstruction.h"
namespace Nom
{
	namespace Runtime
	{
		class ReadFieldInstruction : public NomValueInstruction
		{
		public:
			const RegIndex Register;
			const RegIndex Receiver;
			const ConstantID FieldName;
			const ConstantID ReceiverType; //class
			ReadFieldInstruction(const RegIndex reg, const RegIndex receiver, const ConstantID fieldName, const ConstantID type);
			~ReadFieldInstruction();

			// Inherited via NomValueInstruction
			virtual void Compile(NomBuilder& builder, CompileEnv* env, int lineno) override;

			// Inherited via NomValueInstruction
			virtual void Print(bool resolve = false) override;

			virtual void FillConstantDependencies(NOM_CONSTANT_DEPENCENCY_CONTAINER& result) override;
		};
	}
}



