#pragma once
#include "NomInstruction.h"
#include "CompileEnv.h"
#include "llvm/IR/Value.h"


namespace Nom
{
	namespace Runtime
	{
		class NomValueInstruction : public NomInstruction
		{

		public:
			const RegIndex WriteRegister;
			NomValueInstruction(const RegIndex reg, const OpCode opcode) : NomInstruction(opcode), WriteRegister(reg)
			{

			}
			virtual ~NomValueInstruction() override
			{

			}

			void RegisterValue(CompileEnv* env, NomValue value)
			{
				(*env)[WriteRegister] = value;
			}

		};

	}
}


