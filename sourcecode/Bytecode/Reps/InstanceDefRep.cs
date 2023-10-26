using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using System.IO;
using Nom.Language;

namespace Nom.Bytecode
{
    public class InstanceDefRep : TypeChecker.AParameterized, IInstanceSpec
    {
        public InstanceDefRep( IConstantRef<TypeListConstant> parameters, Visibility visibility, IEnumerable<IInstruction> instructions, int regcount, IConstantRef<StringConstant> name, IParametersSpec p)
        {
            
            ParametersConstant = parameters;
            Visibility = visibility;
            Instructions = instructions;
            RegisterCount = regcount;
            NameConstant = name;
            pp = p;
        }

        public IConstantRef<StringConstant> NameConstant
        {
            get;
        }
        public IParametersSpec pp {
            get;
        }
        public int RegisterCount { get; }

        public Visibility Visibility { get; }
        INamespaceSpec IMember.Container => Container;
        public IClassSpec Container { get; }
        public readonly IEnumerable<IInstruction> Instructions;

        public IConstantRef<TypeListConstant> ParametersConstant { get; }

        protected override IOptional<IParameterizedSpec> ParamParent => Container.InjectOptional();

        public override ITypeParametersSpec TypeParameters => throw new NotImplementedException();

        public IParametersSpec Parameters => pp;

        public IType ReturnType => throw new NotImplementedException();

        public string Name
        {
            get
            {
                return NameConstant.Constant.Value;
            }
        }

        public void WriteByteCode(Stream ws)
        {
            ws.WriteByte((byte)BytecodeInternalElementType.Instance);
            ws.WriteValue(ParametersConstant.ConstantID);
            ws.WriteValue(RegisterCount);
            ws.WriteValue(Instructions.Aggregate((UInt64)0, (acc, i) => acc + i.InstructionCount));
         //   ws.WriteValue((UInt64)Parameters.Entries.Count());
            foreach (IInstruction i in Instructions)
            {
                i.WriteByteCode(ws);
            }
        }
        public static InstanceDefRep Read(Language.IClassSpec container, Stream s, IReadConstantSource rcs)
        {
            byte tag = s.ReadActualByte();
            if (tag != (byte)BytecodeInternalElementType.Instance)
            {
                throw new NomBytecodeException("Bytecode malformed!");
            }
           
            var argsconst = rcs.ReferenceTypeListConstant(s.ReadULong());
         
            var regcount = s.ReadInt();
            var instcount = s.ReadULong();
            var pcount = s.ReadULong();


            List<IInstruction> instruction = new List<IInstruction>();
            List<int> superConstructorArgs = new List<int>();

            for (ulong i = 0; i < instcount; i++)
            {
                instruction.Add(AInstruction.ReadInstruction(s, rcs));
            }
           
            //TODO: actually put visibility in bytecode here
            return new InstanceDefRep( argsconst, Visibility.Public, instruction, regcount, null,null);
        }
    

 }
}
