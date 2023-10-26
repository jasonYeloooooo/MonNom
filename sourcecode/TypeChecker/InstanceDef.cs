using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using Nom.Language;

namespace Nom.TypeChecker
{
    internal class InstanceDef : AParameterized,IInstanceDef
    {
        public InstanceDef(Parser.Identifier name, ITypeParametersSpec typeParameters, IParametersSpec parameters, IType returnType, Visibility visibility, TDClass container)
        {

            Identifier = name;
            TypeParameters = typeParameters;
            Parameters = parameters;
            ReturnType = returnType;
            Visibility = visibility;
            Container = container;
            foreach (var tp in typeParameters)
            {
                tp.Parent = this;
            }

        }

        INamespaceSpec IMember.Container => Container;

        public  IClassSpec Container { get; }
        public Parser.Identifier Identifier { get; }

        public override ITypeParametersSpec TypeParameters { get; }

        public IParametersSpec Parameters { get; }

        public IType ReturnType { get; }

        public Visibility Visibility { get; }
        public IEnumerable<IInstruction> Instructions { get; set; }

        protected override IOptional<IParameterizedSpec> ParamParent => Container.InjectOptional(); //Container.InjectOptional();
        public int RegisterCount { get; set; }

        public string Name => Identifier.Name;
    }
}
