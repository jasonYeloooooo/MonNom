using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Nom.Parser
{
    public abstract class AOpDefaultExpr : AAstNode, IDefaultExpr
    {
        public readonly IDefaultExpr Left;
        public readonly IDefaultExpr Right;
        protected AOpDefaultExpr(IDefaultExpr left, IDefaultExpr right, ISourceSpan locs) : base(locs ?? left.Start.SpanTo(right.End))
        {
            this.Left = left;
            this.Right = right;
        }

        public IEnumerable<Identifier> FreeVars
        {
            get
            {
                return Left.FreeVars.Concat(Right.FreeVars);
            }
        }
        public virtual bool IsAtomic
        {
            get
            {
                return true;
            }
        }


        public override R VisitAstNode<S, R>(IAstNodeVisitor<S, R> visitor, S state)
        {
            return Visit(visitor, state);
        }

        public abstract R Visit<S, R>(IDefaultExprVisitor<S, R> visitor, S state);

        public R Visit<S, R>(IExprVisitor<S, R> visitor, S state)
        {
            return Visit(visitor, state);
        }

        public IType TypeAnnotation { get; set; }
    }
}
