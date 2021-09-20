
function Sift(n : number, s : Stream) : Stream {
  var fst : number = s.first;
  while(fst % n == 0) {
    s= s.rest();
    fst = s.first;
  }
  return new Stream(fst, function () : Stream { return Sift(n, s.rest()); });
}
