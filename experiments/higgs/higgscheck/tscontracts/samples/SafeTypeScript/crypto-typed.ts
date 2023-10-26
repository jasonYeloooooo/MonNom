/*
 * Copyright (c) 2003-2005  Tom Wu
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL TOM WU BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR NOT ADVISED OF
 * THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF LIABILITY, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * In addition, the following condition applies:
 *
 * All redistributions must retain an intact copy of this copyright notice
 * and disclaimer.
 */
module CryptoVERSION {
    // Bits per digit
    var dbits:number;
    var BI_DB:number;
    var BI_DM:number;
    var BI_DV:number;

    var BI_FP:number;
    var BI_FV:number;
    var BI_F1:number;
    var BI_F2:number;

    // JavaScript engine analysis
    var canary = 0xdeadbeefcafe;
    var j_lm = ((canary&0xffffff)==0xefcafe);

    // (public) this & a
    function op_and(x:number,y:number) { return x&y; }
    // (public) this | a
    function op_or(x:number,y:number) { return x|y; }
    // (public) this ^ a
    function op_xor(x:number,y:number) { return x^y; }
    // (public) this & ~a
    function op_andnot(x:number,y:number) { return x&~y; }
    // return index of lowest 1-bit in x, x < 2^31
    function lbit(x:number) {
        if(x == 0) return -1;
        var r = 0;
        if((x&0xffff) == 0) { x >>= 16; r += 16; }
        if((x&0xff) == 0) { x >>= 8; r += 8; }
        if((x&0xf) == 0) { x >>= 4; r += 4; }
        if((x&3) == 0) { x >>= 2; r += 2; }
        if((x&1) == 0) ++r;
        return r;
    }
    // return number of 1 bits in x
    function cbit(x:number) {
        var r = 0;
        while(x != 0) { x &= x-1; ++r; }
        return r;
    }

    var lowprimes : number[] = [2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509];
    var lplim = (1<<26)/lowprimes[lowprimes.length-1];

    // Undo PKCS#1 (type 2, random) padding and, if valid, return the plaintext
    function pkcs1unpad2(d:BigInteger,n:number) {
        var b = d.toByteArray();
        var i = 0;
        while(i < b.length && b[i] == 0) ++i;
        if(b.length-i != n-1 || b[i] != 2)
            return null;
        ++i;
        while(b[i] != 0)
            if(++i >= b.length) return null;
        var ret = "";
        while(++i < b.length)
            ret += String.fromCharCode(b[i]);
        return ret;
    }


    // Basic JavaScript BN library - subset useful for RSA encryption.
    class BigInteger {
        public array : number[] = null;
        public t = 0;
        public s = 0;
        // "constants"
        public static ZERO : BigInteger = null;//nbv(0);
        public static ONE : BigInteger = null;//nbv(1);

        // (public) Constructor
        constructor(a?:any,b?:any,c?:any) {
            this.array = new Array<number>(0);
            if(a != null) {
                if("number" == typeof a) this.fromNumber(a,<number>b,c);
                else if(b == null && "string" != typeof a) this.fromString(a,256); 
                else this.fromString(a,b);
            }
        }


        // am: Compute w_j += (x*this_i), propagate carries,
        // c is initial carry, returns final carry.
        // c < 3*dvalue, x < 2*dvalue, this_i < dvalue
        // We need to select the fastest one that works in this environment.

        // am1: use a single mult and divide to get the high bits,
        // max digit bits should be 26 because
        // max internal value = 2*dvalue^2-2*dvalue (< 2^53)
        private am1(i:number,x:number,w:BigInteger,j:number,c:number,n:number) {
            var this_array = this.array;
            var w_array    = w.array;
            while(--n >= 0) {
                var v = x*this_array[i++]+w_array[j]+c;
                c = Math.floor(v/0x4000000);
                w_array[j++] = v&0x3ffffff;
            }
            return c;
        }

        // am2 avoids a big mult-and-extract completely.
        // Max digit bits should be <= 30 because we do bitwise ops
        // on values up to 2*hdvalue^2-hdvalue-1 (< 2^31)
        private am2(i: number, x: number, w: BigInteger, j: number, c: number, n: number) {
            var this_array = this.array;
            var w_array    = w.array;
            var xl = x&0x7fff, xh = x>>15;
            while(--n >= 0) {
                var l = this_array[i]&0x7fff;
                var h = this_array[i++]>>15;
                var m = xh*l+h*xl;
                l = xl*l+((m&0x7fff)<<15)+w_array[j]+(c&0x3fffffff);
                c = (l>>>30)+(m>>>15)+xh*h+(c>>>30);
                w_array[j++] = l&0x3fffffff;
            }
            return c;
        }

        // Alternately, set max digit bits to 28 since some
        // browsers slow down when dealing with 32-bit numbers.
        private am3(i: number, x: number, w: BigInteger, j: number, c: number, n: number) {
            var this_array = this.array;
            var w_array    = w.array;

            var xl = x&0x3fff, xh = x>>14;
            while(--n >= 0) {
                var l = this_array[i]&0x3fff;
                var h = this_array[i++]>>14;
                var m = xh*l+h*xl;
                l = xl*l+((m&0x3fff)<<14)+w_array[j]+c;
                c = (l>>28)+(m>>14)+xh*h;
                w_array[j++] = l&0xfffffff;
            }
            return c;
        }

        // This is tailored to VMs with 2-bit tagging. It makes sure
        // that all the computations stay within the 29 bits available.
        private am4(i: number, x: number, w: BigInteger, j: number, c: number, n: number) {
            var this_array = this.array;
            var w_array    = w.array;

            var xl = x&0x1fff, xh = x>>13;
            while(--n >= 0) {
                var l = this_array[i]&0x1fff;
                var h = this_array[i++]>>13;
                var m = xh*l+h*xl;
                l = xl*l+((m&0x1fff)<<13)+w_array[j]+c;
                c = (l>>26)+(m>>13)+xh*h;
                w_array[j++] = l&0x3ffffff;
            }
            return c;
        }

        public static setup = 3;

        public am(i: number, x: number, w: BigInteger, j: number, c: number, n: number) {
            switch (BigInteger.setup) {
                case 1: return this.am1(i,x,w,j,c,n);
                case 2: return this.am2(i,x,w,j,c,n);
                case 3: return this.am3(i,x,w,j,c,n);
                case 4: return this.am4(i,x,w,j,c,n);
                default: return this.am3(i,x,w,j,c,n);
            } 
        }

        // (protected) copy this to r
        public copyTo(r:BigInteger) {
            var this_array = this.array;
            var r_array    = r.array;

            for(var i = this.t-1; i >= 0; --i) r_array[i] = this_array[i];
            r.t = this.t;
            r.s = this.s;
        }

        // (protected) set from integer value x, -DV <= x < DV
        public fromInt(x:number) { //needs to be accessed by nbv
            var this_array = this.array;
            this.t = 1;
            this.s = (x<0)?-1:0;
            if(x > 0) this_array[0] = x;
            else if(x < -1) this_array[0] = x+BI_DV;//NS: was DV, should be BI_DV?
            else this.t = 0;
        }

        // (protected) set from string and radix
        public fromString(s:any/*string or number[]*/,b:number) {
            var this_array = this.array;
            var k = 0;
            if(b == 16) k = 4;
            else if(b == 8) k = 3;
            else if(b == 256) k = 8; // byte array
            else if(b == 2) k = 1;
            else if(b == 32) k = 5;
            else if(b == 4) k = 2;
            else { this.fromRadix(s,b); return; }
            this.t = 0;
            this.s = 0;
            var i = s.length, mi = false;
            var sh = 0;
            while(--i >= 0) {
                var x = (k==8)?s[i]&0xff:intAt(s,i);
                if(x < 0) {
                    if(s.charAt(i) == "-") mi = true;
                    continue;
                }
                mi = false;
                if(sh == 0)
                    this_array[this.t++] = x;
                else if(sh+k > BI_DB) {
                    this_array[this.t-1] |= (x&((1<<(BI_DB-sh))-1))<<sh;
                    this_array[this.t++] = (x>>(BI_DB-sh));
                }
                else
                    this_array[this.t-1] |= x<<sh;
                sh += k;
                if(sh >= BI_DB) sh -= BI_DB;
            }
            if(k == 8 && (s[0]&0x80) != 0) {
                this.s = -1;
                if(sh > 0) this_array[this.t-1] |= ((1<<(BI_DB-sh))-1)<<sh;
            }
            this.clamp();
            if(mi) BigInteger.ZERO.subTo(this,this);
        }

        // (protected) clamp off excess high words
        public clamp() {
            var this_array = this.array;
            var c = this.s&BI_DM;
            while(this.t > 0 && this_array[this.t-1] == c) --this.t;
        }

        // (public) return string representation in given radix
        public toString(b:number) {
            var this_array = this.array;
            if(this.s < 0) return "-"+this.negate().toString(b);
            var k:number = 0;
            if(b == 16) k = 4;
            else if(b == 8) k = 3;
            else if(b == 2) k = 1;
            else if(b == 32) k = 5;
            else if(b == 4) k = 2;
            else return this.toRadix(b);
            var km = (1<<k)-1, d:number, m = false, r = "", i = this.t;
            var p = BI_DB-(i*BI_DB)%k;
            if(i-- > 0) {
                if(p < BI_DB && (d = this_array[i]>>p) > 0) { m = true; r = int2char(d); }
                while(i >= 0) {
                    if(p < k) {
                        d = (this_array[i]&((1<<p)-1))<<(k-p);
                        d |= this_array[--i]>>(p+=BI_DB-k);
                    }
                    else {
                        d = (this_array[i]>>(p-=k))&km;
                        if(p <= 0) { p += BI_DB; --i; }
                    }
                    if(d > 0) m = true;
                    if(m) r += int2char(d);
                }
            }
            return m?r:"0";
        }

        // (public) -this
        public negate() { var r = nbi(); BigInteger.ZERO.subTo(this,r); return r; }

        // (public) |this|
        public abs() { return (this.s<0)?this.negate():this; }

        // (public) return + if this > a, - if this < a, 0 if equal
        public compareTo(a:BigInteger) {
            var this_array = this.array;
            var a_array = a.array;

            var r = this.s-a.s;
            if(r != 0) return r;
            var i = this.t;
            r = i-a.t;
            if(r != 0) return r;
            while(--i >= 0) if((r=this_array[i]-a_array[i]) != 0) return r;
            return 0;
        }

        // (public) return the number of bits in "this"
        public bitLength() {
            var this_array = this.array;
            if(this.t <= 0) return 0;
            return BI_DB*(this.t-1)+nbits(this_array[this.t-1]^(this.s&BI_DM));
        }

        // (protected) r = this << n*DB
        public dlShiftTo(n:number,r:BigInteger) { //NS:needs to be public
            var this_array = this.array;
            var r_array = r.array;
            var i:number;
            for(i = this.t-1; i >= 0; --i) r_array[i+n] = this_array[i];
            for(i = n-1; i >= 0; --i) r_array[i] = 0;
            r.t = this.t+n;
            r.s = this.s;
        }

        // (protected) r = this >> n*DB
        public drShiftTo(n:number,r:BigInteger) {
            var this_array = this.array;
            var r_array = r.array;
            for(var i = n; i < this.t; ++i) r_array[i-n] = this_array[i];
            r.t = Math.max(this.t-n,0);
            r.s = this.s;
        }

        // (protected) r = this << n
        public lShiftTo(n: number, r: BigInteger) {
            var this_array = this.array;
            var r_array = r.array;
            var bs = n%BI_DB;
            var cbs = BI_DB-bs;
            var bm = (1<<cbs)-1;
            var ds = Math.floor(n/BI_DB), c = (this.s<<bs)&BI_DM, i:number;
            for(i = this.t-1; i >= 0; --i) {
                r_array[i+ds+1] = (this_array[i]>>cbs)|c;
                c = (this_array[i]&bm)<<bs;
            }
            for(i = ds-1; i >= 0; --i) r_array[i] = 0;
            r_array[ds] = c;
            r.t = this.t+ds+1;
            r.s = this.s;
            r.clamp();
        }

        // (protected) r = this >> n
        private rShiftTo(n: number, r: BigInteger) {
            var this_array = this.array;
            var r_array = r.array;
            r.s = this.s;
            var ds = Math.floor(n/BI_DB);
            if(ds >= this.t) { r.t = 0; return; }
            var bs = n%BI_DB;
            var cbs = BI_DB-bs;
            var bm = (1<<bs)-1;
            r_array[0] = this_array[ds]>>bs;
            for(var i = ds+1; i < this.t; ++i) {
                r_array[i-ds-1] |= (this_array[i]&bm)<<cbs;
                r_array[i-ds] = this_array[i]>>bs;
            }
            if(bs > 0) r_array[this.t-ds-1] |= (this.s&bm)<<cbs;
            r.t = this.t-ds;
            r.clamp();
        }

        // (protected) r = this - a
        public subTo(a: BigInteger, r: BigInteger) { 
            var this_array = this.array;
            var r_array = r.array;
            var a_array = a.array;
            var i = 0, c = 0, m = Math.min(a.t,this.t);
            while(i < m) {
                c += this_array[i]-a_array[i];
                r_array[i++] = c&BI_DM;
                c >>= BI_DB;
            }
            if(a.t < this.t) {
                c -= a.s;
                while(i < this.t) {
                    c += this_array[i];
                    r_array[i++] = c&BI_DM;
                    c >>= BI_DB;
                }
                c += this.s;
            }
            else {
                c += this.s;
                while(i < a.t) {
                    c -= a_array[i];
                    r_array[i++] = c&BI_DM;
                    c >>= BI_DB;
                }
                c -= a.s;
            }
            r.s = (c<0)?-1:0;
            if(c < -1) r_array[i++] = BI_DV+c;
            else if(c > 0) r_array[i++] = c;
            r.t = i;
            r.clamp();
        }

        // (protected) r = this * a, r != this,a (HAC 14.12)
        // "this" should be the larger one if appropriate.
        public multiplyTo(a: BigInteger, r: BigInteger) {
            var this_array = this.array;
            var r_array = r.array;
            var x = this.abs(), y = a.abs();
            var y_array = y.array;

            var i = x.t;
            r.t = i+y.t;
            while(--i >= 0) r_array[i] = 0;
            for(i = 0; i < y.t; ++i) r_array[i+x.t] = x.am(0,y_array[i],r,i,0,x.t);
            r.s = 0;
            r.clamp();
            if(this.s != a.s) BigInteger.ZERO.subTo(r,r);
        }

        // (protected) r = this^2, r != this (HAC 14.16)
        public squareTo(r:BigInteger) {
            var x = this.abs();
            var x_array = x.array;
            var r_array = r.array;

            var i = r.t = 2*x.t;
            while(--i >= 0) r_array[i] = 0;
            for(i = 0; i < x.t-1; ++i) {
                var c = x.am(i,x_array[i],r,2*i,0,1);
                if((r_array[i+x.t]+=x.am(i+1,2*x_array[i],r,2*i+1,c,x.t-i-1)) >= BI_DV) {
                    r_array[i+x.t] -= BI_DV;
                    r_array[i+x.t+1] = 1;
                }
            }
            if(r.t > 0) r_array[r.t-1] += x.am(i,x_array[i],r,2*i,0,1);
            r.s = 0;
            r.clamp();
        }

        // (protected) divide this by m, quotient and remainder to q, r (HAC 14.20)
        // r != q, this != m.  q or r may be null.
        public divRemTo(m:BigInteger,q:BigInteger,r:BigInteger) { //NS: needs to be public
            var pm = m.abs();
            if(pm.t <= 0) return;
            var pt = this.abs();
            if(pt.t < pm.t) {
                if(q != null) q.fromInt(0);
                if(r != null) this.copyTo(r);
                return;
            }
            if(r == null) r = nbi();
            var y = nbi(), ts = this.s, ms = m.s;
            var pm_array = pm.array;
            var nsh = BI_DB-nbits(pm_array[pm.t-1]);	// normalize modulus
            if(nsh > 0) { pm.lShiftTo(nsh,y); pt.lShiftTo(nsh,r); }
            else { pm.copyTo(y); pt.copyTo(r); }
            var ys = y.t;

            var y_array = y.array;
            var y0 = y_array[ys-1];
            if(y0 == 0) return;
            var yt = y0*(1<<BI_F1)+((ys>1)?y_array[ys-2]>>BI_F2:0);
            var d1 = BI_FV/yt, d2 = (1<<BI_F1)/yt, e = 1<<BI_F2;
            var i = r.t, j = i-ys, t = (q==null)?nbi():q;
            y.dlShiftTo(j,t);

            var r_array = r.array;
            if(r.compareTo(t) >= 0) {
                r_array[r.t++] = 1;
                r.subTo(t,r);
            }
            BigInteger.ONE.dlShiftTo(ys,t);
            t.subTo(y,y);	// "negative" y so we can replace sub with am later
            while(y.t < ys) y_array[y.t++] = 0;
            while(--j >= 0) {
                // Estimate quotient digit
                var qd = (r_array[--i]==y0)?BI_DM:Math.floor(r_array[i]*d1+(r_array[i-1]+e)*d2);
                if((r_array[i]+=y.am(0,qd,r,j,0,ys)) < qd) {	// Try it out
                    y.dlShiftTo(j,t);
                    r.subTo(t,r);
                    while(r_array[i] < --qd) r.subTo(t,r);
                }
            }
            if(q != null) {
                r.drShiftTo(ys,q);
                if(ts != ms) BigInteger.ZERO.subTo(q,q);
            }
            r.t = ys;
            r.clamp();
            if(nsh > 0) r.rShiftTo(nsh,r);	// Denormalize remainder
            if(ts < 0) BigInteger.ZERO.subTo(r,r);
        }

        // (public) this mod a
        public mod(a:BigInteger) {
            var r = nbi();
            this.abs().divRemTo(a,null,r);
            if(this.s < 0 && r.compareTo(BigInteger.ZERO) > 0) a.subTo(r,r);
            return r;
        }

        // (protected) return "-1/this % 2^DB"; useful for Mont. reduction
        // justification:
        //         xy == 1 (mod m)
        //         xy =  1+km
        //   xy(2-xy) = (1+km)(1-km)
        // x[y(2-xy)] = 1-k^2m^2
        // x[y(2-xy)] == 1 (mod m^2)
        // if y is 1/x mod m, then y(2-xy) is 1/x mod m^2
        // should reduce x and y(2-xy) by m^2 at each step to keep size bounded.
        // JS multiply "overflows" differently from C/C++, so care is needed here.
        public invDigit() {
            var this_array = this.array;
            if(this.t < 1) return 0;
            var x = this_array[0];
            if((x&1) == 0) return 0;
            var y = x&3;		// y == 1/x mod 2^2
            y = (y*(2-(x&0xf)*y))&0xf;	// y == 1/x mod 2^4
            y = (y*(2-(x&0xff)*y))&0xff;	// y == 1/x mod 2^8
            y = (y*(2-(((x&0xffff)*y)&0xffff)))&0xffff;	// y == 1/x mod 2^16
            // last step - calculate inverse mod DV directly;
            // assumes 16 < DB <= 32 and assumes ability to handle 48-bit ints
            y = (y*(2-x*y%BI_DV))%BI_DV;		// y == 1/x mod 2^dbits
            // we really want the negative inverse, and -DV < y < DV
            return (y>0)?BI_DV-y:-y;
        }

        // (protected) true iff this is even
        private isEven() {
            var this_array = this.array;
            return ((this.t>0)?(this_array[0]&1):this.s) == 0;
        }

        // (protected) this^e, e < 2^32, doing sqr and mul with "r" (HAC 14.79)
        private exp(e:number,z:ModularReducer) {
            if(e > 0xffffffff || e < 1) return BigInteger.ONE;
            var r = nbi(), r2 = nbi(), g = z.convert(this), i = nbits(e)-1;
            g.copyTo(r);
            while(--i >= 0) {
                z.sqrTo(r,r2);
                if((e&(1<<i)) > 0) z.mulTo(r2,g,r);
                else { var t = r; r = r2; r2 = t; }
            }
            return z.revert(r);
        }

        // (public) this^e % m, 0 <= e < 2^32
        public modPowInt(e:number,m:BigInteger) {
            var z:ModularReducer = null;
            if(e < 256 || m.isEven()) z = new Classic(m); else z = new Montgomery(m);
            return this.exp(e,z);
        }

        // Copyright (c) 2005  Tom Wu
        // All Rights Reserved.
        // See "LICENSE" for details.

        // Extended JavaScript BN functions, required for RSA private ops.

        // (public)
        public clone() { var r = nbi(); this.copyTo(r); return r; }

        // (public) return value as integer
        public intValue() {
            var this_array = this.array;
            if(this.s < 0) {
                if(this.t == 1) return this_array[0]-BI_DV;
                else if(this.t == 0) return -1;
            }
            else if(this.t == 1) return this_array[0];
            else if(this.t == 0) return 0;
            // assumes 16 < DB < 32
            return ((this_array[1]&((1<<(32-BI_DB))-1))<<BI_DB)|this_array[0];
        }

        // (public) return value as byte
        public byteValue() {
            var this_array = this.array;
            return (this.t==0)?this.s:(this_array[0]<<24)>>24;
        }

        // (public) return value as short (assumes DB>=16)
        public shortValue() {
            var this_array = this.array;
            return (this.t==0)?this.s:(this_array[0]<<16)>>16;
        }

        // (protected) return x s.t. r^x < DV
        private chunkSize(r:number) { return Math.floor(Math.LN2*BI_DB/Math.log(r)); }

        // (public) 0 if this == 0, 1 if this > 0
        public signum() {
            var this_array = this.array;
            if(this.s < 0) return -1;
            else if(this.t <= 0 || (this.t == 1 && this_array[0] <= 0)) return 0;
            else return 1;
        }

        // (protected) convert to radix string
        private toRadix(b:number=10) {
            if(this.signum() == 0 || b < 2 || b > 36) return "0";
            var cs = this.chunkSize(b);
            var a = Math.pow(b,cs);
            var d = nbv(a), y = nbi(), z = nbi(), r = "";
            this.divRemTo(d,y,z);
            while(y.signum() > 0) {
                r = (a+z.intValue()).toString(b).substr(1) + r;
                y.divRemTo(d,y,z);
            }
            return z.intValue().toString(b) + r;
        }

        // (protected) convert from radix string
        private fromRadix(s:string,b:number=10) {
            this.fromInt(0);
            var cs = this.chunkSize(b);
            var d = Math.pow(b,cs), mi = false, j = 0, w = 0;
            for(var i = 0; i < s.length; ++i) {
                var x = intAt(s,i);
                if(x < 0) {
                    if(s.charAt(i) == "-" && this.signum() == 0) mi = true;
                    continue;
                }
                w = b*w+x;
                if(++j >= cs) {
                    this.dMultiply(d);
                    this.dAddOffset(w,0);
                    j = 0;
                    w = 0;
                }
            }
            if(j > 0) {
                this.dMultiply(Math.pow(b,j));
                this.dAddOffset(w,0);
            }
            if(mi) BigInteger.ZERO.subTo(this,this);
        }

        // (protected) alternate constructor
        private fromNumber(a:number,b:any,c?:any) {
            if("number" == typeof b) {
                // new BigInteger(int,int,RNG)
                if(a < 2) this.fromInt(1);
                else {
                    this.fromNumber(a,c);
                    if(!this.testBit(a-1))	// force MSB set
                        this.bitwiseTo(BigInteger.ONE.shiftLeft(a-1),op_or,this);
                    if(this.isEven()) this.dAddOffset(1,0); // force odd
                    while(!this.isProbablePrime(<number>b)) {
                        this.dAddOffset(2,0);
                        if(this.bitLength() > a) this.subTo(BigInteger.ONE.shiftLeft(a-1),this);
                    }
                }
            }
            else {
                //throw new Error("This branch doesn't look safe");
                // new BigInteger(int,RNG)
                var x:number[] = new Array<number>(), t = a&7;
                x.length = (a>>3)+1;
                b.nextBytes(x);
                if(t > 0) x[0] &= ((1<<t)-1); else x[0] = 0;
                this.fromString(x,256);
            }
        }

        // (public) convert to bigendian byte array
        public toByteArray() {
            var this_array = this.array;
            var i = this.t, r: number[] = new Array<number>();
            r[0] = this.s;
            var p = BI_DB-(i*BI_DB)%8, d:number,  k = 0;
            if(i-- > 0) {
                if(p < BI_DB && (d = this_array[i]>>p) != (this.s&BI_DM)>>p)
                    r[k++] = d|(this.s<<(BI_DB-p));
                while(i >= 0) {
                    if(p < 8) {
                        d = (this_array[i]&((1<<p)-1))<<(8-p);
                        d |= this_array[--i]>>(p+=BI_DB-8);
                    }
                    else {
                        d = (this_array[i]>>(p-=8))&0xff;
                        if(p <= 0) { p += BI_DB; --i; }
                    }
                    if((d&0x80) != 0) d |= -256;
                    if(k == 0 && (this.s&0x80) != (d&0x80)) ++k;
                    if(k > 0 || d != this.s) r[k++] = d;
                }
            }
            return r;
        }

        public equals(a:BigInteger) { return(this.compareTo(a)==0); }
        public min(a: BigInteger) { return(this.compareTo(a)<0)?this:a; }
        public max(a: BigInteger) { return(this.compareTo(a)>0)?this:a; }

        // (protected) r = this op a (bitwise)
        private bitwiseTo(a: BigInteger, op:(a:number,b:number) => number, r: BigInteger) {
            var this_array = this.array;
            var a_array    = a.array;
            var r_array    = r.array;
            var i:number, f:number, m = Math.min(a.t,this.t);
            for(i = 0; i < m; ++i) r_array[i] = op(this_array[i],a_array[i]);
            if(a.t < this.t) {
                f = a.s&BI_DM;
                for(i = m; i < this.t; ++i) r_array[i] = op(this_array[i],f);
                r.t = this.t;
            }
            else {
                f = this.s&BI_DM;
                for(i = m; i < a.t; ++i) r_array[i] = op(f,a_array[i]);
                r.t = a.t;
            }
            r.s = op(this.s,a.s);
            r.clamp();
        }


        public and(a: BigInteger) { var r = nbi(); this.bitwiseTo(a,op_and,r); return r; }
        public ar(a: BigInteger) { var r = nbi(); this.bitwiseTo(a,op_or,r); return r; }
        public xor(a: BigInteger) { var r = nbi(); this.bitwiseTo(a,op_xor,r); return r; }
        public andNot(a: BigInteger) { var r = nbi(); this.bitwiseTo(a,op_andnot,r); return r; }
        
        // (public) ~this
        public not() {
            var this_array = this.array;
            var r = nbi();
            var r_array = r.array;

            for(var i = 0; i < this.t; ++i) r_array[i] = BI_DM&~this_array[i];
            r.t = this.t;
            r.s = ~this.s;
            return r;
        }

        // (public) this << n
        public shiftLeft(n: number) {
            var r = nbi();
            if(n < 0) this.rShiftTo(-n,r); else this.lShiftTo(n,r);
            return r;
        }

        // (public) this >> n
        public shiftRight(n: number) {
            var r = nbi();
            if(n < 0) this.lShiftTo(-n,r); else this.rShiftTo(n,r);
            return r;
        }

        // (public) returns index of lowest 1-bit (or -1 if none)
        public getLowestSetBit() {
            var this_array = this.array;
            for(var i = 0; i < this.t; ++i)
                if(this_array[i] != 0) return i*BI_DB+lbit(this_array[i]);
            if(this.s < 0) return this.t*BI_DB;
            return -1;
        }

        // (public) return number of set bits
        public bitCount() {
            var this_array = this.array; //NS: fix--was missing
            var r = 0, x = this.s&BI_DM;
            for(var i = 0; i < this.t; ++i) r += cbit(this_array[i]^x);
            return r;
        }

        // (public) true iff nth bit is set
        public testBit(n: number) {
            var this_array = this.array;
            var j = Math.floor(n/BI_DB);
            if(j >= this.t) return(this.s!=0);
            return((this_array[j]&(1<<(n%BI_DB)))!=0);
        }

        // (protected) this op (1<<n)
        private changeBit(n: number,op:(a:number, b:number) => number) {
            var r = BigInteger.ONE.shiftLeft(n);
            this.bitwiseTo(r,op,r);
            return r;
        }

        // (public) this | (1<<n)
        public setBit(n: number) { return this.changeBit(n,op_or); }

        // (public) this & ~(1<<n)
        public clearBit(n: number) { return this.changeBit(n,op_andnot); }

        // (public) this ^ (1<<n)
        public flipBit(n: number) { return this.changeBit(n,op_xor); }

        // (protected) r = this + a
        private addTo(a:BigInteger,r:BigInteger) {
            var this_array = this.array;
            var a_array = a.array;
            var r_array = r.array;
            var i = 0, c = 0, m = Math.min(a.t,this.t);
            while(i < m) {
                c += this_array[i]+a_array[i];
                r_array[i++] = c&BI_DM;
                c >>= BI_DB;
            }
            if(a.t < this.t) {
                c += a.s;
                while(i < this.t) {
                    c += this_array[i];
                    r_array[i++] = c&BI_DM;
                    c >>= BI_DB;
                }
                c += this.s;
            }
            else {
                c += this.s;
                while(i < a.t) {
                    c += a_array[i];
                    r_array[i++] = c&BI_DM;
                    c >>= BI_DB;
                }
                c += a.s;
            }
            r.s = (c<0)?-1:0;
            if(c > 0) r_array[i++] = c;
            else if(c < -1) r_array[i++] = BI_DV+c;
            r.t = i;
            r.clamp();
        }

        // (public) this + a
        public add(a:BigInteger) { var r = nbi(); this.addTo(a,r); return r; }

        // (public) this - a
        public subtract(a: BigInteger) { var r = nbi(); this.subTo(a,r); return r; }

        // (public) this * a
        public multiply(a: BigInteger) { var r = nbi(); this.multiplyTo(a,r); return r; }

        // (public) this / a
        public divide(a: BigInteger) { var r = nbi(); this.divRemTo(a,r,null); return r; }

        // (public) this % a
        public remainder(a: BigInteger) { var r = nbi(); this.divRemTo(a,null,r); return r; }

        // (public) [this/a,this%a]
        public divideAndRemainder(a: BigInteger) {
            var q = nbi(), r = nbi();
            this.divRemTo(a,q,r);
            return new Array<BigInteger>(q,r);//overloading
        }

        // (protected) this *= n, this >= 0, 1 < n < DV
        private dMultiply(n:number) {
            var this_array = this.array;
            this_array[this.t] = this.am(0,n-1,this,0,0,this.t);
            ++this.t;
            this.clamp();
        }

        // (protected) this += n << w words, this >= 0
        public dAddOffset(n:number,w:number) {
            var this_array = this.array;
            while(this.t <= w) this_array[this.t++] = 0;
            this_array[w] += n;
            while(this_array[w] >= BI_DV) {
                this_array[w] -= BI_DV;
                if(++w >= this.t) this_array[this.t++] = 0;
                ++this_array[w];
            }
        }

        // (public) this^e
        public pow(e:number) { return this.exp(e,new NullExp()); }

        // (protected) r = lower n words of "this * a", a.t <= n
        // "this" should be the larger one if appropriate.
        public multiplyLowerTo(a: BigInteger, n:number, r: BigInteger) {
            var r_array = r.array;
            var a_array = a.array;
            var i = Math.min(this.t+a.t,n);
            r.s = 0; // assumes a,this >= 0
            r.t = i;
            while(i > 0) r_array[--i] = 0;
            var j;
            for(j = r.t-this.t; i < j; ++i) r_array[i+this.t] = this.am(0,a_array[i],r,i,0,this.t);
            for(j = Math.min(a.t,n); i < j; ++i) this.am(0,a_array[i],r,i,0,n-i);
            r.clamp();
        }

        // (protected) r = "this * a" without lower n words, n > 0
        // "this" should be the larger one if appropriate.
        public multiplyUpperTo(a: BigInteger, n: number, r: BigInteger)  {
            var r_array = r.array;
            var a_array = a.array;
            --n;
            var i = r.t = this.t+a.t-n;
            r.s = 0; // assumes a,this >= 0
            while(--i >= 0) r_array[i] = 0;
            for(i = Math.max(n-this.t,0); i < a.t; ++i)
                r_array[this.t+i-n] = this.am(n-i,a_array[i],r,0,0,this.t+i-n);
            r.clamp();
            r.drShiftTo(1,r);
        }

        // (public) this^e % m (HAC 14.85)
        public modPow(e:BigInteger,m:BigInteger) {
            var e_array = e.array;
            var i = e.bitLength(), k:number, r = nbv(1), z:ModularReducer;
            if(i <= 0) return r;
            else if(i < 18) k = 1;
            else if(i < 48) k = 3;
            else if(i < 144) k = 4;
            else if(i < 768) k = 5;
            else k = 6;
            if(i < 8)
                z = new Classic(m);
            else if(m.isEven())
                z = new Barrett(m);
            else
                z = new Montgomery(m);

            // precomputation 
            var g : BigInteger[] = new Array<BigInteger>(), n = 3, k1 = k-1, km = (1<<k)-1;
            g[1] = z.convert(this);
            if(k > 1) {
                var g2 = nbi();
                z.sqrTo(g[1],g2);
                while(n <= km) {
                    g[n] = nbi();
                    z.mulTo(g2,g[n-2],g[n]);
                    n += 2;
                }
            }

            var j = e.t-1, w:number, is1 = true, r2 = nbi(), t:BigInteger;
            i = nbits(e_array[j])-1;
            while(j >= 0) {
                if(i >= k1) w = (e_array[j]>>(i-k1))&km;
                else {
                    w = (e_array[j]&((1<<(i+1))-1))<<(k1-i);
                    if(j > 0) w |= e_array[j-1]>>(BI_DB+i-k1);
                }

                n = k;
                while((w&1) == 0) { w >>= 1; --n; }
                if((i -= n) < 0) { i += BI_DB; --j; }
                if(is1) {	// ret == 1, don't bother squaring or multiplying it
                    g[w].copyTo(r);
                    is1 = false;
                }
                else {
                    while(n > 1) { z.sqrTo(r,r2); z.sqrTo(r2,r); n -= 2; }
                    if(n > 0) z.sqrTo(r,r2); else { t = r; r = r2; r2 = t; }
                    z.mulTo(r2,g[w],r);
                }

                while(j >= 0 && (e_array[j]&(1<<i)) == 0) {
                    z.sqrTo(r,r2); t = r; r = r2; r2 = t;
                    if(--i < 0) { i = BI_DB-1; --j; }
                }
            }
            return z.revert(r);
        }

        // (public) gcd(this,a) (HAC 14.54)
        public gcd(a: BigInteger) {
            var x = (this.s<0)?this.negate():this.clone();
            var y = (a.s<0)?a.negate():a.clone();
            if(x.compareTo(y) < 0) { var t = x; x = y; y = t; }
            var i = x.getLowestSetBit(), g = y.getLowestSetBit();
            if(g < 0) return x;
            if(i < g) g = i;
            if(g > 0) {
                x.rShiftTo(g,x);
                y.rShiftTo(g,y);
            }
            while(x.signum() > 0) {
                if((i = x.getLowestSetBit()) > 0) x.rShiftTo(i,x);
                if((i = y.getLowestSetBit()) > 0) y.rShiftTo(i,y);
                if(x.compareTo(y) >= 0) {
                    x.subTo(y,x);
                    x.rShiftTo(1,x);
                }
                else {
                    y.subTo(x,y);
                    y.rShiftTo(1,y);
                }
            }
            if(g > 0) y.lShiftTo(g,y);
            return y;
        }

        // (protected) this % n, n < 2^26
        private modInt(n:number) {
            var this_array = this.array;
            if(n <= 0) return 0;
            var d = BI_DV%n, r = (this.s<0)?n-1:0;
            if(this.t > 0)
                if(d == 0) r = this_array[0]%n;
            else for(var i = this.t-1; i >= 0; --i) r = (d*r+this_array[i])%n;
            return r;
        }

        // (public) 1/this % m (HAC 14.61)
        public modInverse(m: BigInteger) {
            var ac = m.isEven();
            if((this.isEven() && ac) || m.signum() == 0) return BigInteger.ZERO;
            var u = m.clone(), v = this.clone();
            var a = nbv(1), b = nbv(0), c = nbv(0), d = nbv(1);
            while(u.signum() != 0) {
                while(u.isEven()) {
                    u.rShiftTo(1,u);
                    if(ac) {
                        if(!a.isEven() || !b.isEven()) { a.addTo(this,a); b.subTo(m,b); }
                        a.rShiftTo(1,a);
                    }
                    else if(!b.isEven()) b.subTo(m,b);
                    b.rShiftTo(1,b);
                }
                while(v.isEven()) {
                    v.rShiftTo(1,v);
                    if(ac) {
                        if(!c.isEven() || !d.isEven()) { c.addTo(this,c); d.subTo(m,d); }
                        c.rShiftTo(1,c);
                    }
                    else if(!d.isEven()) d.subTo(m,d);
                    d.rShiftTo(1,d);
                }
                if(u.compareTo(v) >= 0) {
                    u.subTo(v,u);
                    if(ac) a.subTo(c,a);
                    b.subTo(d,b);
                }
                else {
                    v.subTo(u,v);
                    if(ac) c.subTo(a,c);
                    d.subTo(b,d);
                }
            }
            if(v.compareTo(BigInteger.ONE) != 0) return BigInteger.ZERO;
            if(d.compareTo(m) >= 0) return d.subtract(m);
            if(d.signum() < 0) d.addTo(m,d); else return d;
            if(d.signum() < 0) return d.add(m); else return d;
        }
        
        // (public) test primality with certainty >= 1-.5^t
        public isProbablePrime(t:number) {
            var i, x = this.abs();
            var x_array = x.array;
            if(x.t == 1 && x_array[0] <= lowprimes[lowprimes.length-1]) {
                for(i = 0; i < lowprimes.length; ++i)
                    if(x_array[0] == lowprimes[i]) return true;
                return false;
            }
            if(x.isEven()) return false;
            i = 1;
            while(i < lowprimes.length) {
                var m = lowprimes[i], j = i+1;
                while(j < lowprimes.length && m < lplim) m *= lowprimes[j++];
                m = x.modInt(m);
                while(i < j) if(m%lowprimes[i++] == 0) return false;
            }
            return x.millerRabin(t);
        }

        // (protected) true if probably prime (HAC 4.24, Miller-Rabin)
        private millerRabin(t:number) {
            var n1 = this.subtract(BigInteger.ONE);
            var k = n1.getLowestSetBit();
            if(k <= 0) return false;
            var r = n1.shiftRight(k);
            t = (t+1)>>1;
            if(t > lowprimes.length) t = lowprimes.length;
            var a = nbi();
            for(var i = 0; i < t; ++i) {
                a.fromInt(lowprimes[i]);
                var y = a.modPow(r,this);
                if(y.compareTo(BigInteger.ONE) != 0 && y.compareTo(n1) != 0) {
                    var j = 1;
                    while(j++ < k && y.compareTo(n1) != 0) {
                        y = y.modPowInt(2,this);
                        if(y.compareTo(BigInteger.ONE) == 0) return false;
                    }
                    if(y.compareTo(n1) != 0) return false;
                }
            }
            return true;
        }
    }

    // am3/28 is best for SM, Rhino, but am4/26 is best for v8.
    // Kestrel (Opera 9.5) gets its best result with am4/26.
    // IE7 does 9% better with am3/28 than with am4/26.
    // Firefox (SM) gets 10% faster with am3/28 than with am4/26.

    var setupEngine = function(setup:number, bits:number) {
        BigInteger.setup = setup;
        BigInteger.ZERO = nbv(0);
        BigInteger.ONE = nbv(1);
        // BigInteger.prototype.am = fn;
        dbits = bits;

        BI_DB = dbits;
        BI_DM = ((1<<dbits)-1);
        BI_DV = (1<<dbits);

        BI_FP = 52;
        BI_FV = Math.pow(2,BI_FP);
        BI_F1 = BI_FP-dbits;
        BI_F2 = 2*dbits-BI_FP;
    }


    // return new, unset BigInteger
    function nbi() : BigInteger { return new BigInteger(null); }

    // Digit conversions
    var BI_RM = "0123456789abcdefghijklmnopqrstuvwxyz";
    var BI_RC : number[] = new Array<number>();
    var rr:number=0,vv:number=0;
    rr = "0".charCodeAt(0);
    for(vv = 0; vv <= 9; ++vv) BI_RC[rr++] = vv;
    rr = "a".charCodeAt(0);
    for(vv = 10; vv < 36; ++vv) BI_RC[rr++] = vv;
    rr = "A".charCodeAt(0);
    for(vv = 10; vv < 36; ++vv) BI_RC[rr++] = vv;

    function int2char(n:number) { return BI_RM.charAt(n); }
    function intAt(s:string,i:number) {
        var c = BI_RC[s.charCodeAt(i)];
        return (c==null)?-1:c;
    }

    // return bigint initialized to value
    function nbv(i:number) { var r = nbi(); r.fromInt(i); return r; }


    // returns bit length of the integer x
    function nbits(x:number) {
        var r = 1, t:number;
        if((t=x>>>16) != 0) { x = t; r += 16; }
        if((t=x>>8) != 0) { x = t; r += 8; }
        if((t=x>>4) != 0) { x = t; r += 4; }
        if((t=x>>2) != 0) { x = t; r += 2; }
        if((t=x>>1) != 0) { x = t; r += 1; }
        return r;
    }

    class ModularReducer {
        constructor () {}
        public convert(x: BigInteger) : BigInteger {
            throw new Error("Abstract method");
        }
        public revert(x: BigInteger) : BigInteger {
            throw new Error("Abstract method");
        }
        public reduce(x: BigInteger) : void {
            throw new Error("Abstract method");
        }
        public mulTo(x: BigInteger, y: BigInteger, r: BigInteger) : void {
            throw new Error("Abstract method");
        }
        public sqrTo(x: BigInteger, y: BigInteger):  void {
            throw new Error("Abstract method");
        }
    }

    // A "null" reducer
    class NullExp extends ModularReducer {
        constructor() {
            super(); 
        }
        public nop(x) { return x; }
        public mulTo(x: BigInteger, y: BigInteger, r: BigInteger) { x.multiplyTo(y, r); }
        public sqrTo(x: BigInteger, r: BigInteger) { x.squareTo(r); }
        public convert(x: BigInteger): BigInteger {
            throw new Error("Null reducer");
        }
        public revert(x: BigInteger): BigInteger {
            throw new Error("Null reducer");
        }
        public reduce(x: BigInteger): void {
            throw new Error("Null reducer");
        }
    }

    class Classic extends ModularReducer {
        // Modular reduction using "classic" algorithm
        constructor(public m:BigInteger) {
            super();
        }
        public convert(x:BigInteger) {
            if(x.s < 0 || x.compareTo(this.m) >= 0) return x.mod(this.m);
            else return x;
        }
        
        public revert(x:BigInteger) { return x; }
        public reduce(x:BigInteger) { x.divRemTo(this.m,null,x); }
        public mulTo(x: BigInteger, y: BigInteger, r: BigInteger) { x.multiplyTo(y,r); this.reduce(r); }
        public sqrTo(x: BigInteger, r: BigInteger) { x.squareTo(r); this.reduce(r); }
    }


    // Montgomery reduction
    class Montgomery extends ModularReducer {
        public mp:number=0;
        public mpl:number=0;
        public mph:number=0;
        public um:number=0;
        public mt2:number=0;

        constructor(public m:BigInteger) {
            super();
            this.mp = this.m.invDigit();
            this.mpl = this.mp&0x7fff;
            this.mph = this.mp>>15;
            this.um = (1<<(BI_DB-15))-1;
            this.mt2 = 2*this.m.t;
        }

        // xR mod m
        public convert(x: BigInteger) {
            var r = nbi();
            x.abs().dlShiftTo(this.m.t,r);
            r.divRemTo(this.m,null,r);
            if(x.s < 0 && r.compareTo(BigInteger.ZERO) > 0) this.m.subTo(r,r);
            return r;
        }

        // x/R mod m
        public revert(x:BigInteger) {
            var r = nbi();
            x.copyTo(r);
            this.reduce(r);
            return r;
        }

        // x = x/R mod m (HAC 14.32)
        public reduce(x: BigInteger) {
            var x_array = x.array;
            while(x.t <= this.mt2)	// pad x so am has enough room later
                x_array[x.t++] = 0;
            for(var i = 0; i < this.m.t; ++i) {
                // faster way of calculating u0 = x[i]*mp mod DV
                var j = x_array[i]&0x7fff;
                var u0 = (j*this.mpl+(((j*this.mph+(x_array[i]>>15)*this.mpl)&this.um)<<15))&BI_DM;
                // use am to combine the multiply-shift-add into one call
                j = i+this.m.t;
                x_array[j] += this.m.am(0,u0,x,i,0,this.m.t);
                // propagate carry
                while(x_array[j] >= BI_DV) { x_array[j] -= BI_DV; x_array[++j]++; }
            }
            x.clamp();
            x.drShiftTo(this.m.t,x);
            if(x.compareTo(this.m) >= 0) x.subTo(this.m,x);
        }

        // r = "x^2/R mod m"; x != r
        public sqrTo(x: BigInteger, r: BigInteger) { x.squareTo(r); this.reduce(r); }

        // r = "xy/R mod m"; x,y != r
        public mulTo(x: BigInteger, y: BigInteger, r: BigInteger) { x.multiplyTo(y,r); this.reduce(r); }
    }


    // Barrett modular reduction
    class Barrett extends ModularReducer {
        public r2:BigInteger=null;
        public q3:BigInteger=null;
        public mu:BigInteger=null;
        
        constructor(public m: BigInteger) {
            super();
            // setup Barrett
            this.r2 = nbi();
            this.q3 = nbi();
            BigInteger.ONE.dlShiftTo(2*this.m.t,this.r2);
            this.mu = this.r2.divide(this.m);
        }

        public convert(x: BigInteger) {
            if(x.s < 0 || x.t > 2*this.m.t) return x.mod(this.m);
            else if(x.compareTo(this.m) < 0) return x;
            else { var r = nbi(); x.copyTo(r); this.reduce(r); return r; }
        }

        public revert(x: BigInteger) { return x; }

        // x = x mod m (HAC 14.42)
        public reduce(x: BigInteger) {
            x.drShiftTo(this.m.t-1,this.r2);
            if(x.t > this.m.t+1) { x.t = this.m.t+1; x.clamp(); }
            this.mu.multiplyUpperTo(this.r2,this.m.t+1,this.q3);
            this.m.multiplyLowerTo(this.q3,this.m.t+1,this.r2);
            while(x.compareTo(this.r2) < 0) x.dAddOffset(1,this.m.t+1);
            x.subTo(this.r2,x);
            while(x.compareTo(this.m) >= 0) x.subTo(this.m,x);
        }

        // r = x^2 mod m; x != r
        public sqrTo(x: BigInteger, r: BigInteger) { x.squareTo(r); this.reduce(r); }

        // r = x*y mod m; x,y != r
        public mulTo(x: BigInteger, y: BigInteger, r: BigInteger) { x.multiplyTo(y,r); this.reduce(r); }
    }

    // BigInteger interfaces not implemented in jsbn:

    // BigInteger(int signum, byte[] magnitude)
    // double doubleValue()
    // float floatValue()
    // int hashCode()
    // long longValue()
    // static BigInteger valueOf(long val)
    // prng4.js - uses Arcfour as a PRNG

    class Arcfour {
        public i = 0;
        public j = 0;
        public S : number [] = new Array<number>();
        constructor() {}

        // Initialize arcfour context from key, an array of ints, each from [0..255]
        public init(key:number[]) {
            var i=0, j=0, t=0;
            for(i = 0; i < 256; ++i)
                this.S[i] = i;
            j = 0;
            for(i = 0; i < 256; ++i) {
                j = (j + this.S[i] + key[i % key.length]) & 255;
                t = this.S[i];
                this.S[i] = this.S[j];
                this.S[j] = t;
            }
            this.i = 0;
            this.j = 0;
        }

        public next() {
            var t=0;
            this.i = (this.i + 1) & 255;
            this.j = (this.j + this.S[this.i]) & 255;
            t = this.S[this.i];
            this.S[this.i] = this.S[this.j];
            this.S[this.j] = t;
            return this.S[(t + this.S[this.i]) & 255];
        }
    }

    // Plug in your RNG constructor here
    function prng_newstate() {
        return new Arcfour();
    }

    // Pool size must be a multiple of 4 and greater than 32.
    // An array of bytes the size of the pool will be passed to init()
    var rng_psize = 256;
    // Random number generator - requires a PRNG backend, e.g. prng4.js

    // For best results, put code like
    // <body onClick='rng_seed_time();' onKeyPress='rng_seed_time();'>
    // in your main HTML document.

    var rng_state:Arcfour=null;
    var rng_pool:number[]=null;
    var rng_pptr=0;

    // Mix in a 32-bit integer into the pool
    function rng_seed_int(x) {
        rng_pool[rng_pptr++] ^= x & 255;
        rng_pool[rng_pptr++] ^= (x >> 8) & 255;
        rng_pool[rng_pptr++] ^= (x >> 16) & 255;
        rng_pool[rng_pptr++] ^= (x >> 24) & 255;
        if(rng_pptr >= rng_psize) rng_pptr -= rng_psize;
    }

    // Mix in the current time (w/milliseconds) into the pool
    function rng_seed_time() {
        // Use pre-computed date to avoid making the benchmark
        // results dependent on the current date.
        rng_seed_int(1122926989487);
    }

    // Initialize the pool with junk if needed.
    if(rng_pool == null) {
        rng_pool = new Array<number>();
        rng_pptr = 0;
        var t;
        while(rng_pptr < rng_psize) {  // extract some randomness from Math.random()
            t = Math.floor(65536 * Math.random());
            rng_pool[rng_pptr++] = t >>> 8;
            rng_pool[rng_pptr++] = t & 255;
        }
        rng_pptr = 0;
        rng_seed_time();
        //rng_seed_int(window.screenX);
        //rng_seed_int(window.screenY);
    }

    function rng_get_byte() {
        if(rng_state == null) {
            rng_seed_time();
            rng_state = prng_newstate();
            rng_state.init(rng_pool);
            for(rng_pptr = 0; rng_pptr < rng_pool.length; ++rng_pptr)
                rng_pool[rng_pptr] = 0;
            rng_pptr = 0;
            //rng_pool = null;
        }
        // TODO: allow reseeding after first request
        return rng_state.next();
    }

    function rng_get_bytes(ba:number[]) {
        for(var i = 0; i < ba.length; ++i) ba[i] = rng_get_byte();
    }

    class SecureRandom {
        constructor() {}
        public nextBytes(ba:number[]) { 
            return rng_get_bytes(ba); 
        }
    }

    // SecureRandom.prototype.nextBytes = rng_get_bytes;
    // Depends on jsbn.js and rng.js

    // convert a (hex) string to a bignum object
    function parseBigInt(str:string,r:number) {
        return new BigInteger(str,r);
    }

    function linebrk(s:string,n:number) {
        var ret = "";
        var i = 0;
        while(i + n < s.length) {
            ret += s.substring(i,i+n) + "\n";
            i += n;
        }
        return ret + s.substring(i,s.length);
    }

    function byte2Hex(b:number) {
        if(b < 0x10)
            return "0" + b.toString(16);
        else
            return b.toString(16);
    }

    // PKCS#1 (type 2, random) pad input string s to n bytes, and return a bigint
    function pkcs1pad2(s:string,n:number) {
        if(n < s.length + 11) {
            console.log("Message too long for RSA");
            return null;
        }
        var ba : number[] = new Array<number>();
        var i = s.length - 1;
        while(i >= 0 && n > 0) ba[--n] = s.charCodeAt(i--);
        ba[--n] = 0;
        var rng = new SecureRandom();
        var x : number[] = new Array<number>();
        while(n > 2) { // random non-zero pad
            x[0] = 0;
            while(x[0] == 0) rng.nextBytes(x);
            ba[--n] = x[0];
        }
        ba[--n] = 2;
        ba[--n] = 0;
        return new BigInteger(ba);
    }

    // "empty" RSA key constructor
    class RSAKey {
        public n : BigInteger = null;
        public e = 0;
        public d : BigInteger = null;
        public p: BigInteger = null;
        public q: BigInteger = null;
        public dmp1: BigInteger = null;
        public dmq1: BigInteger = null;
        public coeff: BigInteger = null;
        constructor() { }


        // Set the public key fields N and e from hex strings
        public setPublic(N:string,E:string) {
            if(N != null && E != null && N.length > 0 && E.length > 0) {
                this.n = parseBigInt(N,16);
                this.e = parseInt(E,16);
            }
            else
                console.log("Invalid RSA public key");
        }

        // Perform raw public operation on "x": return x^e (mod n)
        private doPublic(x:BigInteger) {
            return x.modPowInt(this.e, this.n);
        }

        // Return the PKCS#1 RSA encryption of "text" as an even-length hex string
        public encrypt(text:string) {
            var m = pkcs1pad2(text,(this.n.bitLength()+7)>>3);
            if(m == null) return null;
            var c = this.doPublic(m);
            if(c == null) return null;
            var h = c.toString(16);
            if((h.length & 1) == 0) return h; else return "0" + h;
        }

        // Return the PKCS#1 RSA encryption of "text" as a Base64-encoded string
        //public encrypt_b64(text) {
        //  var h = this.encrypt(text);
        //  if(h) return hex2b64(h); else return null;
        //}

        // Set the private key fields N, e, and d from hex strings
        public setPrivate(N: string, E: string, D: string) {
            if(N != null && E != null && N.length > 0 && E.length > 0) {
                this.n = parseBigInt(N,16);
                this.e = parseInt(E,16);
                this.d = parseBigInt(D,16);
            }
            else
                console.log("Invalid RSA private key");
        }

        // Set the private key fields N, e, d and CRT params from hex strings
        public setPrivateEx(N: string, E: string, D: string, P: string, Q: string, DP: string, DQ: string, C: string) {
            if(N != null && E != null && N.length > 0 && E.length > 0) {
                this.n = parseBigInt(N,16);
                this.e = parseInt(E,16);
                this.d = parseBigInt(D,16);
                this.p = parseBigInt(P,16);
                this.q = parseBigInt(Q,16);
                this.dmp1 = parseBigInt(DP,16);
                this.dmq1 = parseBigInt(DQ,16);
                this.coeff = parseBigInt(C,16);
            }
            else
                console.log("Invalid RSA private key");
        }

        // Generate a new random private key B bits long, using public expt E
        public generate(B: number, E: string) {
            var rng = new SecureRandom();
            var qs = B>>1;
            this.e = parseInt(E,16);
            var ee = new BigInteger(E,16);
            for(;;) {
                for(;;) {
                    this.p = new BigInteger(B-qs,1,rng);
                    if(this.p.subtract(BigInteger.ONE).gcd(ee).compareTo(BigInteger.ONE) == 0 && this.p.isProbablePrime(10)) break;
                }
                for(;;) {
                    this.q = new BigInteger(qs,1,rng);
                    if(this.q.subtract(BigInteger.ONE).gcd(ee).compareTo(BigInteger.ONE) == 0 && this.q.isProbablePrime(10)) break;
                }
                if(this.p.compareTo(this.q) <= 0) {
                    var t = this.p;
                    this.p = this.q;
                    this.q = t;
                }
                var p1 = this.p.subtract(BigInteger.ONE);
                var q1 = this.q.subtract(BigInteger.ONE);
                var phi = p1.multiply(q1);
                if(phi.gcd(ee).compareTo(BigInteger.ONE) == 0) {
                    this.n = this.p.multiply(this.q);
                    this.d = ee.modInverse(phi);
                    this.dmp1 = this.d.mod(p1);
                    this.dmq1 = this.d.mod(q1);
                    this.coeff = this.q.modInverse(this.p);
                    break;
                }
            }
        }

        // Perform raw private operation on "x": return x^d (mod n)
        private doPrivate(x:BigInteger) {
            if(this.p == null || this.q == null)
                return x.modPow(this.d, this.n);

            // TODO: re-calculate any missing CRT params
            var xp = x.mod(this.p).modPow(this.dmp1, this.p);
            var xq = x.mod(this.q).modPow(this.dmq1, this.q);

            while(xp.compareTo(xq) < 0)
                xp = xp.add(this.p);
            return xp.subtract(xq).multiply(this.coeff).mod(this.p).multiply(this.q).add(xq);
        }

        // Return the PKCS#1 RSA decryption of "ctext".
        // "ctext" is an even-length hex string and the output is a plain string.
        public decrypt(ctext:string) {
            var c = parseBigInt(ctext, 16);
            var m = this.doPrivate(c);
            if(m == null) return null;
            return pkcs1unpad2(m, (this.n.bitLength()+7)>>3);
        }

        // Return the PKCS#1 RSA decryption of "ctext".
        // "ctext" is a Base64-encoded string and the output is a plain string.
        //public b64_decrypt(ctext) {
        //  var h = b64tohex(ctext);
        //  if(h) return this.decrypt(h); else return null;
        //}
    }

    

    var nValue="a5261939975948bb7a58dffe5ff54e65f0498f9175f5a09288810b8975871e99af3b5dd94057b0fc07535f5f97444504fa35169d461d0d30cf0192e307727c065168c788771c561a9400fb49175e9e6aa4e23fe11af69e9412dd23b0cb6684c4c2429bce139e848ab26d0829073351f4acd36074eafd036a5eb83359d2a698d3";
    var eValue="10001";
    var dValue="8e9912f6d3645894e8d38cb58c0db81ff516cf4c7e5a14c7f1eddb1459d2cded4d8d293fc97aee6aefb861859c8b6a3d1dfe710463e1f9ddc72048c09751971c4a580aa51eb523357a3cc48d31cfad1d4a165066ed92d4748fb6571211da5cb14bc11b6e2df7c1a559e6d5ac1cd5c94703a22891464fba23d0d965086277a161";
    var pValue="d090ce58a92c75233a6486cb0a9209bf3583b64f540c76f5294bb97d285eed33aec220bde14b2417951178ac152ceab6da7090905b478195498b352048f15e7d";
    var qValue="cab575dc652bb66df15a0359609d51d1db184750c00c6698b90ef3465c99655103edbf0d54c56aec0ce3c4d22592338092a126a0cc49f65a4a30d222b411e58f";
    var dmp1Value="1a24bca8e273df2f0e47c199bbf678604e7df7215480c77c8db39f49b000ce2cf7500038acfff5433b7d582a01f1826e6f4d42e1c57f5e1fef7b12aabc59fd25";
    var dmq1Value="3d06982efbbe47339e1f6d36b1216b8a741d410b0c662f54f7118b27b9a4ec9d914337eb39841d8666f3034408cf94f5b62f11c402fc994fe15a05493150d9fd";
    var coeffValue="3a3e731acd8960b7ff9eb81a7ff93bd1cfa74cbd56987db58b4594fb09c09084db1734c8143f98b602b981aaa9243ca28deb69b5b280ee8dcee0fd2625e53250";

    setupEngine(3, 28);

    var TEXT = "The quick brown fox jumped over the extremely lazy frog! " +
        "Now is the time for all good men to come to the party.";
    var encrypted : string = null;

    export function encrypt() {
        var RSA = new RSAKey();
        RSA.setPublic(nValue, eValue);
        RSA.setPrivateEx(nValue, eValue, dValue, pValue, qValue, dmp1Value, dmq1Value, coeffValue);
        encrypted = RSA.encrypt(TEXT);
    }

    export function decrypt() {
        var RSA = new RSAKey();
        RSA.setPublic(nValue, eValue);
        RSA.setPrivateEx(nValue, eValue, dValue, pValue, qValue, dmp1Value, dmq1Value, coeffValue);
        var decrypted = RSA.decrypt(encrypted);
        if (decrypted != TEXT) {
            throw new Error("Crypto operation failed");
        }
    }
}

var benchmark_fn = function() { CryptoVERSION.encrypt(); CryptoVERSION.decrypt(); };
var setup_fn = undefined;
var teardown_fn = undefined;
