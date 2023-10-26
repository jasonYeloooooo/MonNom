using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Nom.Parser
{
    public class InstanceFieldAssignment : AStmt
    {
        public readonly Identifier FieldName;
        public readonly IDefaultExpr Expr;

        public override IEnumerable<Identifier> UsedIdentifiers
        {
            get
            {
                return Expr.FreeVars.Cons(FieldName);
            }
        }
        public InstanceFieldAssignment(Identifier fieldName, IDefaultExpr expr, ISourceSpan locs = null) : base(locs)
        {
            FieldName = fieldName;
            Expr = expr;
        }


        public override R VisitAstNode<S, R>(IAstNodeVisitor<S, R> visitor, S state)
        {
            return Visit(visitor, state);
        }

        public override void PrettyPrint(PrettyPrinter p)
        {
            //throw new NotImplementedException();
            // below is assignment the instance value

            FieldName.PrettyPrint(p);
            p.WriteKeyword("=");
            Expr.PrettyPrint(p);
            p.WritePunctuation(";");
            p.WriteWhitespace();
        }

        public override Ret Visit<Arg, Ret>(IStmtVisitor<Arg, Ret> visitor, Arg arg = default(Arg))
        {
            return visitor.VisitInstanceFieldAssignment(this, arg);
        }

        
    }

    public partial interface IStmtVisitor<in S, out R>
    {
        Func<InstanceFieldAssignment, S, R> VisitInstanceFieldAssignment { get; }
    }
}

