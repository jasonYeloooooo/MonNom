﻿using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using Nom.Language;

namespace Nom.TypeChecker.StdLib
{
    public class Object : AParameterized, IClassSpec
    {
        public static Object Instance = new Object();
        private Object() { constructor = new StdLibConstructorSpec(this, new ParametersSpec(new List<IParameterSpec>()), Visibility.Public); }
        public IOptional<IParamRef<IClassSpec, IType>> SuperClass { get; } = Optional<IParamRef<IClassSpec, IType>>.Empty;
        public bool IsFinal => false;

        public bool IsPartial => false;
        public bool IsExpando => false;

        public IEnumerable<IStaticMethodSpec> StaticMethods
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<IFieldSpec> Fields
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<IStaticFieldSpec> StaticFields
        {
            get
            {
                yield break;
            }
        }

        private StdLibConstructorSpec constructor;
        public IEnumerable<IConstructorSpec> Constructors
        {
            get
            {
                yield return constructor;
                yield break;
            }
        }

        public IEnumerable<IInstanceSpec> Instances
        {
            get
            {
                yield break;
            }
        }

        public bool IsShape => false;

        public Visibility Visibility => Visibility.Public;

        public ILibrary Library => StdLib.Instance;

        public IEnumerable<IParamRef<IInterfaceSpec, IType>> Implements
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<IMethodSpec> Methods
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<INamespaceSpec> Children
        {
            get
            {
                yield break;
            }
        }

        public IOptional<INamespaceSpec> ParentNamespace => StdLibGlobalNamespace.Instance.InjectOptional();
        protected override IOptional<IParameterizedSpec> ParamParent => ParentNamespace;

        public string Name => "Object";

        public override ITypeParametersSpec TypeParameters => TypeParametersSpec.Empty;

        public IEnumerable<INamespaceSpec> Namespaces
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<IInterfaceSpec> Interfaces
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<IClassSpec> Classes
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<INamespaceSpec> PublicChildren
        {
            get
            {
                yield break;
            }
        }

        public IEnumerable<INamespaceSpec> ProtectedChildren
        {
            get
            {
                yield break;
            }
        }

        public string FullQualifiedName => Name+"_0";

        public Ret Visit<Arg, Ret>(INamespaceSpecVisitor<Arg, Ret> visitor, Arg arg = default(Arg))
        {
            return visitor.VisitClassSpec(this, arg);
        }

        public IParamRef<IClassSpec, IType> MakeClassRef(ITypeEnvironment<IType> env)
        {
            return new ClassRef<Language.IType>(this, env);
        }
        public INamedType Instantiate(ITypeEnvironment<ITypeArgument> args)
        {
            return new ClassType(this, args);
        }
    }
}
