using System;
using System.Collections.Generic;
using System.Text;
using Nom.Language;

namespace Nom.TypeChecker
{
    public interface IInstanceDef : IInstanceSpec
    {
        IEnumerable<IInstruction> Instructions { get; }
        int RegisterCount { get; }
    }
}
