using System;
using System.Collections.Generic;
using System.Text;

namespace Nom.Language
{
    public interface IInstanceSpec : ICallableSpec, IMember
    {

        String Name
        {
            get;
        }
        new IClassSpec Container { get; }
      
     
    }
}
