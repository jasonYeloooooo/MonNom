#pragma once
#include <memory>
#include <vector>
#include "NomTypeDecls.h"
#include "CompileEnv.h"

namespace Nom
{
	namespace Runtime
	{
		class NomSubstitutionContext;
		class NomSignature
		{
		public:
			NomTypeRef ReturnType;
			std::vector<NomTypeRef> ArgumentTypes;
			std::vector<NomTypeParameterRef> TypeArguments;
			NomSignature();
			NomSignature(const std::vector<NomTypeParameterRef>&& typeArgs, const std::vector<NomTypeRef>&& argTypes, const NomTypeRef returnType);

			~NomSignature();

			NomSignature Substitute(const NomSubstitutionContext* context) const;

			bool SatisfiesArguments(const NomSignature& other, bool optimistic = false) const;

			bool Satisfies(const NomSignature &other, bool optimistic = false) const;

			//Returns true iff any argument or return type could possibly contain a primitive but isn't guaranteed to
			bool HasPrimitiveUncertainty() const;
		};
	}
}