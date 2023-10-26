using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Nom.Parser
{
    public class InstanceDef : AAstNode
    {
        public readonly Identifier Name;
        public readonly Identifier SuperName;
        public readonly IEnumerable<IDefaultExpr> SuperArgs;
        public readonly IEnumerable<InstanceFieldAssignment> Assignments;
        public readonly IEnumerable<ArgDecl> Arguments;
        public Block code;

        //check the type of the argument not working......
        public IEnumerable<TypeArgDecl> TypeArguments { get; set; } = new List<TypeArgDecl>();
        
        public readonly bool IsMulti;
        public readonly bool IsDefault;
        public readonly VisibilityNode Visibility;
        public InstanceDef(VisibilityNode visibility, Identifier name, Identifier superName, IEnumerable<InstanceFieldAssignment> assignments, bool isMulti, bool isDefault, IEnumerable<IDefaultExpr> superArgs, IEnumerable<ArgDecl> arguments, ISourceSpan locs = null) : base(locs)
        {
            Visibility = visibility;
            SuperName = superName;
            Name = name;
            Assignments = assignments.ToList();
           
            code = new Block(Assignments, locs);
          
            IsMulti = isMulti;
            IsDefault = isDefault;
            this.Arguments = (arguments ?? new List<ArgDecl>()).ToList();
            this.SuperArgs = (superArgs ?? new List<IDefaultExpr>()).ToList();
            if (Arguments.Count() > 0 && !isMulti)
            {
                CompilerOutput.RegisterException(new ParseException("Only multi-instance instances can have arguments!", Locs.AsSourceLocs()));
            }
            if (Arguments.Count() > 0 && isDefault)
            {
                CompilerOutput.RegisterException(new ParseException("A default instance must not depend on arguments!", Locs.AsSourceLocs()));
            }
        }

        public override void PrettyPrint(PrettyPrinter p)
        {
            Visibility.PrettyPrint(p);
            p.WriteKeyword("instance");
            p.IncreaseIndent();
            Name.PrettyPrint(p);
            p.DecreaseIndent();
            //Check is it multi, if yes it will have the 
            if (IsMulti)
            {
                p.WritePunctuation("(");
                p.IncreaseIndent();
                p.PrintMembers(Arguments, ",");
                p.WritePunctuation(")");
                p.DecreaseIndent();
            }
            if (SuperName != null)
            {
                p.WriteKeyword(" extends");
                p.IncreaseIndent();
                SuperName.PrettyPrint(p);
                p.DecreaseIndent();
                p.WritePunctuation("(");
                p.IncreaseIndent();
                p.PrintMembers(SuperArgs, ",");
                p.WritePunctuation(")");
                p.DecreaseIndent();
            }
            p.WriteLine();
            p.WritePunctuation("{");
            p.IncreaseIndent();
            p.WriteLine();
            p.PrintMembers(Assignments, ",");
            p.WriteLine();

            p.DecreaseIndent();
            p.WritePunctuation("}");
        }

        public override R VisitAstNode<S, R>(IAstNodeVisitor<S, R> visitor, S state)
        {
            return visitor.VisitInstanceDef(this, state);
        }

    }

    public partial interface IAstNodeVisitor<in S, out R>
    {
        Func<InstanceDef, S, R> VisitInstanceDef { get; }
    }
}

