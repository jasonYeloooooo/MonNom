using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Nom.TypeChecker
{
    public class CallInstanceCheckedInstruction : AValueInstruction
    {
        public Language.IParameterizedSpecRef<Language.IInstanceSpec> Instance { get; }
        public IEnumerable<IRegister> Arguments { get; }
        public CallInstanceCheckedInstruction(Language.IParameterizedSpecRef<Language.IInstanceSpec> instance, IEnumerable<IRegister> arguments, IRegister register) : base(register)
        {
            Instance = instance;
            Arguments = arguments;
        }

        public IEnumerable<Language.IType> ActualParameters
        {
            get
            {
                return Instance.GetOrderedArgumentList();
            }
        }

        public override Ret Visit<Arg, Ret>(IInstructionVisitor<Arg, Ret> visitor, Arg arg = default)
        {
            return visitor.VisitCallInstanceCheckedInstruction(this, arg);
        }

    }

        public partial interface IInstructionVisitor<in Arg, out Ret>
        {
            Func<CallInstanceCheckedInstruction, Arg, Ret> VisitCallInstanceCheckedInstruction { get; }
        }
    
}
