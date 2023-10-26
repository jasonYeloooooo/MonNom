#include "NomMemberContext.h"
#include "NomConstants.h"
#include "NomTypeVar.h"
#include "NomAlloc.h"
#include "NomTypeParameter.h"
#include <iostream>

namespace Nom
{
	namespace Runtime
	{
		NomMemberContextLoaded::NomMemberContextLoaded(const NomMemberContext *parent, ConstantID typeParametersID) : parent(parent), typeParametersID(typeParametersID)
		{
		}

		NomMemberContextLoaded::~NomMemberContextLoaded()
		{
		}

		size_t NomMemberContextLoaded::GetDirectTypeParametersCount() const
		{
			if (initialized)
			{
				return this->TypeParameters.size();
			}
			else
			{
				NomTypeParametersConstant* ntlc = NomConstants::GetTypeParameters(typeParametersID);
				return ntlc->GetSize(); 
			}
		}
		NomTypeParameterRef NomMemberContextLoaded::GetLocalTypeParameter(int index) const
		{
			return GetDirectTypeParameters()[index];
		}
		const llvm::ArrayRef<NomTypeParameterRef> NomMemberContextLoaded::GetDirectTypeParameters() const
		{
			if (!initialized)
			{
				NomTypeParametersConstant* ntlc = NomConstants::GetTypeParameters(typeParametersID);
				llvm::ArrayRef<NomTypeParameterConstant*> tparams = ntlc->GetParameters();
				size_t varcount = tparams.size();
				NomTypeParameterLoaded* vars = (NomTypeParameterLoaded *) nmalloc(sizeof(NomTypeParameterLoaded) * varcount);
				NomTypeParameterRef* varptrs = new NomTypeParameterRef [varcount];
				for (size_t i = 0; i < varcount; i++)
				{
					varptrs[i] = &vars[i];
					new(vars + i) NomTypeParameterLoaded(this, i, tparams[i]->UpperBound, tparams[i]->LowerBound);
				}
				this->TypeParameters = llvm::ArrayRef<NomTypeParameterRef>(varptrs, varcount);
				initialized = true;
			}
			return this->TypeParameters;
		}

		size_t NomMemberContext::GetTypeParametersCount() const
		{
			size_t ret = GetDirectTypeParametersCount();
			if (GetParent() != nullptr)
			{
				ret += GetParent()->GetTypeParametersCount();
			}
			return ret;
		}

		size_t NomMemberContext::GetTypeParametersStart() const
		{
			if (GetParent() != nullptr)
			{
				return GetParent()->GetTypeParametersCount();
			}
			return 0;
		}


		NomTypeParameterRef NomMemberContext::GetTypeParameter(int index) const
		{
			int modifiedIndex = index - GetTypeParametersStart();
			if (modifiedIndex >= 0)
			{
				return GetLocalTypeParameter(modifiedIndex);
			}
			return GetParent()->GetTypeParameter(index);
		}

		const llvm::ArrayRef<NomTypeRef> NomMemberContext::GetDirectTypeVariables() const
		{
			if (directVariables.data() == nullptr)
			{
				auto directParams = GetDirectTypeParameters();
				NomTypeRef* tarr = (NomTypeRef*)(nmalloc(sizeof(NomTypeRef) * directParams.size()));
				for (size_t i = 0; i < directParams.size(); i++)
				{
					tarr[i] = new NomTypeVar(directParams[i]);
				}
				directVariables = llvm::ArrayRef<NomTypeRef>(tarr, directParams.size());
			}
			return directVariables;
		}

		//SimpleNomMemberContext::SimpleNomMemberContext(const NomMemberContext* parent, ConstantID typeParametersID, const std::string& symbolName) : NomMemberContext(parent, typeParametersID), symbolName(symbolName)
		//{
		//}

		//SimpleNomMemberContext::SimpleNomMemberContext(const NomMemberContext* parent, llvm::ArrayRef<NomTypeVarRef> typeParameters, const std::string& symbolName) : NomMemberContext(parent, typeParameters), symbolName(symbolName)
		//{
		//}

		//const std::string* SimpleNomMemberContext::GetSymbolName() const
		//{
		//	return &symbolName;
		//}

		const llvm::ArrayRef<NomTypeRef> NomMemberContext::GetAllTypeVariables() const
		{
			if (allVariables.data() == nullptr)
			{
				size_t pos = GetTypeParametersCount();
				NomTypeRef* tarr = (NomTypeRef*)(nmalloc(sizeof(NomTypeRef) * pos));
				const NomMemberContext* nmc = this;
				while (pos > 0)
				{
					auto ltvars = nmc->GetDirectTypeVariables();
					auto lpos = ltvars.size();
					while (lpos > 0)
					{
						pos--;
						lpos--;
						tarr[pos] = ltvars[lpos];
					}
					nmc = nmc->GetParent();
				}
				allVariables = llvm::ArrayRef<NomTypeRef>(tarr, GetTypeParametersCount());
			}
			return allVariables;
		}

		const llvm::ArrayRef<NomTypeParameterRef> NomMemberContext::GetAllTypeParameters() const
		{
			auto parent = GetParent();
			if (parent == nullptr)
			{
				return GetDirectTypeParameters();
			}
			if (allParameters.data() == nullptr)
			{
				if (GetDirectTypeParametersCount() == 0)
				{
					allParameters = parent->GetAllTypeParameters();
					return allParameters;
				}
				size_t count = GetTypeParametersCount();
				NomTypeParameterRef* parr = (NomTypeParameterRef*)(nmalloc(sizeof(NomTypeParameterRef) * count));
				for (size_t i = 0; i < count; i++)
				{
					parr[i] = GetTypeParameter(i);
				}
				allParameters = llvm::ArrayRef<NomTypeParameterRef>(parr, count);
			}
			return allParameters;
		}
	}
}