using System;
using System.Collections.Generic;
using System.Text;

namespace Nom.Parser
{
    public class DefaultExprStmt : AStmt
    {
        private IDefaultExpr expr;
        public IDefaultExpr Expression => expr;
        public DefaultExprStmt(IDefaultExpr e, ISourceSpan locs) : base(locs)
        {
            this.expr = e;
        }

        public override IEnumerable<Identifier> UsedIdentifiers
        {
            get
            {
                return expr.FreeVars;
            }
        }



        public override void PrettyPrint(PrettyPrinter p)
        {
            Expression.PrettyPrint(p);
            p.WritePunctuation(";");
            p.WriteWhitespace();
        }

        public override Ret Visit<Arg, Ret>(IStmtVisitor<Arg, Ret> visitor, Arg arg = default)
        {
            return visitor.VisitDefaultExprStmt(this, arg);
        }
    }

    public partial interface IStmtVisitor<in S, out R>
    {
        Func<DefaultExprStmt, S, R> VisitDefaultExprStmt { get; }
    }
    
}
