using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Nom.Parser
{
    public class BinOpDefaultExpr : AOpDefaultExpr
    {
        public BinaryOperatorNode Operator
        {
            get;
            private set;
        }
        public BinOpDefaultExpr(IDefaultExpr left, BinaryOperatorNode op, IDefaultExpr right, ISourceSpan locs = null) : base(left, right, locs)
        {
            this.Operator = op;
        }

        public override R Visit<S, R>(IDefaultExprVisitor<S, R> visitor, S state)
        {
            return visitor.VisitBinOpDefaultExpr(this, state);
        }

        public override void PrettyPrint(PrettyPrinter p)
        {

            p.WriteKeyword("(");
            Left.PrettyPrint(p);
            p.WriteWhitespace();
            Operator.PrettyPrint(p);
            p.WriteWhitespace();
            Right.PrettyPrint(p);
            p.WriteKeyword(")");
            p.WriteWhitespace();

        }

     
    }



    public partial interface IDefaultExprVisitor<in S, out R>
    {
        Func<BinOpDefaultExpr, S, R> VisitBinOpDefaultExpr { get; }
    }
}
