from retic.runtime import *
from retic.monotonic import *
from retic.typing import *
from compat import xrange
import util
from math import sin, cos, sqrt
import optparse
import time


class Point(retic_actual(object), metaclass=Monotonic):
    retic_class_type = Class('Point', {'maximize': Function(NamedParameters([('self', TypeVariable('Point')), ('other', TypeVariable('Point'))]), TypeVariable('Point')), '__init__': Function(NamedParameters([('self', Dyn), ('i', Dyn)]), Dyn), 'normalize': Function(NamedParameters([('self', TypeVariable('Point'))]), Dyn), '__repr__': Function(NamedParameters([('self', TypeVariable('Point'))]), String), }, {'y': Float, 'x': Float, 'z': Float, })

    def __init__(self, i):
        retic_cast(self, Dyn, Object('', {}), '\nbm_float11.py:12:8: Cannot write to attribute x in value %s because it is not an object. (code NON_OBJECT_WRITE)').x = x = retic_cast(sin, Dyn, Function(AnonymousParameters([Dyn]), Dyn), "\nbm_float11.py:12:21: Expected function of type Function(['Dyn'], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)")(i)
        retic_cast(self, Dyn, Object('', {}), '\nbm_float11.py:13:8: Cannot write to attribute y in value %s because it is not an object. (code NON_OBJECT_WRITE)').y = (retic_cast(cos, Dyn, Function(AnonymousParameters([Dyn]), Dyn), "\nbm_float11.py:13:17: Expected function of type Function(['Dyn'], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)")(i) * 3)
        retic_cast(self, Dyn, Object('', {}), '\nbm_float11.py:14:8: Cannot write to attribute z in value %s because it is not an object. (code NON_OBJECT_WRITE)').z = ((x * x) / 2)
    __init__ = retic_cast(__init__, Dyn, Function(NamedParameters([('self', Dyn), ('i', Dyn)]), Dyn), "\nbm_float11.py:11:4: Function %s does not match specified type Function(['self:Dyn', 'i:Dyn'], Dyn). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)")

    def __repr__(self):
        return ('<Point: x=%s, y=%s, z=%s>' % (retic_getattr_static(self, 'x', Float), retic_getattr_static(self, 'y', Float), retic_getattr_static(self, 'z', Float)))
    __repr__ = retic_cast(__repr__, Dyn, Function(NamedParameters([('self', Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, }))]), String), '\nbm_float11.py:16:4: Function %s does not match specified type Function(["self:Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})"], String). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)')

    def normalize(self):
        x = retic_getattr_static(self, 'x', Float)
        y = retic_getattr_static(self, 'y', Float)
        z = retic_getattr_static(self, 'z', Float)
        norm = retic_cast(sqrt, Dyn, Function(AnonymousParameters([Float]), Dyn), "\nbm_float11.py:23:15: Expected function of type Function(['Float'], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)")((((x * x) + (y * y)) + (z * z)))
        retic_setattr_static(self, 'x', retic_cast((retic_getattr_static(self, 'x', Float) / norm), Dyn, Float, '\nbm_float11.py:24: Right hand side of assignment was expected to be of type Float, but value %s provided instead. (code SINGLE_ASSIGN_ERROR)'), Float)
        retic_setattr_static(self, 'y', retic_cast((retic_getattr_static(self, 'y', Float) / norm), Dyn, Float, '\nbm_float11.py:25: Right hand side of assignment was expected to be of type Float, but value %s provided instead. (code SINGLE_ASSIGN_ERROR)'), Float)
        retic_setattr_static(self, 'z', retic_cast((retic_getattr_static(self, 'z', Float) / norm), Dyn, Float, '\nbm_float11.py:26: Right hand side of assignment was expected to be of type Float, but value %s provided instead. (code SINGLE_ASSIGN_ERROR)'), Float)
    normalize = retic_cast(normalize, Dyn, Function(NamedParameters([('self', Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, }))]), Dyn), '\nbm_float11.py:19:4: Function %s does not match specified type Function(["self:Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})"], Dyn). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)')

    def maximize(self, other):
        retic_setattr_static(self, 'x', (retic_getattr_static(self, 'x', Float) if (retic_getattr_static(self, 'x', Float) > retic_getattr_static(other, 'x', Float)) else retic_getattr_static(other, 'x', Float)), Float)
        retic_setattr_static(self, 'y', (retic_getattr_static(self, 'y', Float) if (retic_getattr_static(self, 'y', Float) > retic_getattr_static(other, 'y', Float)) else retic_getattr_static(other, 'y', Float)), Float)
        retic_setattr_static(self, 'z', (retic_getattr_static(self, 'z', Float) if (retic_getattr_static(self, 'z', Float) > retic_getattr_static(other, 'z', Float)) else retic_getattr_static(other, 'z', Float)), Float)
        return self
    maximize = retic_cast(maximize, Dyn, Function(NamedParameters([('self', Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })), ('other', Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, }))]), Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })), '\nbm_float11.py:28:4: Function %s does not match specified type Function(["self:Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})", "other:Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})"], Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)')
Point = retic_cast(Point, Dyn, Class('Point', {'maximize': Function(NamedParameters([('self', TypeVariable('Point')), ('other', TypeVariable('Point'))]), TypeVariable('Point')), '__init__': Function(NamedParameters([('self', Dyn), ('i', Dyn)]), Dyn), 'normalize': Function(NamedParameters([('self', TypeVariable('Point'))]), Dyn), '__repr__': Function(NamedParameters([('self', TypeVariable('Point'))]), String), }, {'y': Float, 'x': Float, 'z': Float, }), "\nbm_float11.py:8:0: Class %s does not match specified type Class(Point, {'maximize': Function(['self:TypeVar(Point)', 'other:TypeVar(Point)'], TypeVar(Point)), '__init__': Function(['self:Dyn', 'i:Dyn'], Dyn), 'normalize': Function(['self:TypeVar(Point)'], Dyn), '__repr__': Function(['self:TypeVar(Point)'], String)}, {'y': Float, 'x': Float, 'z': Float}). Consider changing the type or setting it to Dyn. (code BAD_CLASS_INJECTION)")

def maximize(points):
    next = retic_cast(points[0], Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, }), Dyn, '\nbm_float11.py:36:4: Right hand side of assignment was expected to be of type Dyn, but value %s provided instead. (code SINGLE_ASSIGN_ERROR)')
    for p in points[1:]:
        next = retic_cast(retic_cast(next, Dyn, Object('', {'maximize': Dyn, }), '\nbm_float11.py:38:15: Accessing nonexistant object attribute maximize from value %s. (code WIDTH_DOWNCAST)').maximize, Dyn, Function(AnonymousParameters([Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })]), Dyn), '\nbm_float11.py:38:15: Expected function of type Function(["Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})"], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')(p)
    return retic_cast(next, Dyn, Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, }), "\nbm_float11.py:39:4: A return value of type Object(Point, {'__repr__': Function([], String), 'normalize': Function([], Dyn), 'maximize': Function(['other:TypeVar(Point)'], TypeVar(Point)), 'y': Float, 'x': Float, 'z': Float}) was expected but a value %s was returned instead. (code RETURN_ERROR)")
maximize = retic_cast(maximize, Dyn, Function(NamedParameters([('points', List(Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })))]), Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })), '\nbm_float11.py:35:0: Function %s does not match specified type Function(["points:List(Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float}))"], Object(Point, {\'__repr__\': Function([], String), \'normalize\': Function([], Dyn), \'maximize\': Function([\'other:TypeVar(Point)\'], TypeVar(Point)), \'y\': Float, \'x\': Float, \'z\': Float})). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)')

def benchmark(n):
    points = [retic_cast(Point(i), Dyn, Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, }), "\nbm_float11.py:42:14: Constructed object value %s does not match type Object(Point, {'__repr__': Function([], String), 'normalize': Function([], Dyn), 'maximize': Function(['other:TypeVar(Point)'], TypeVar(Point)), 'y': Float, 'x': Float, 'z': Float}),  expected for instances of Class(Point, {'maximize': Function(['self:TypeVar(Point)', 'other:TypeVar(Point)'], TypeVar(Point)), '__init__': Function(['self:Dyn', 'i:Dyn'], Dyn), 'normalize': Function(['self:TypeVar(Point)'], Dyn), '__repr__': Function(['self:TypeVar(Point)'], String)}, {'y': Float, 'x': Float, 'z': Float}). Consider changing the type or setting it to Dyn. (code BAD_OBJECT_INJECTION)") for i in retic_cast(xrange, Dyn, Function(AnonymousParameters([Int]), Dyn), "\nbm_float11.py:42:32: Expected function of type Function(['Int'], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)")(n)]
    for p in retic_cast(points, List(Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })), List(Dyn), '\nbm_float11.py:43:4: Iteration target was expected to be of type List(Dyn), but value %s was provided instead. (code ITER_ERROR)'):
        retic_cast(retic_cast(p, Dyn, Object('', {'normalize': Dyn, }), '\nbm_float11.py:44:8: Accessing nonexistant object attribute normalize from value %s. (code WIDTH_DOWNCAST)').normalize, Dyn, Function(AnonymousParameters([]), Dyn), '\nbm_float11.py:44:8: Expected function of type Function([], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')()
    return maximize(points)
benchmark = retic_cast(benchmark, Dyn, Function(NamedParameters([('n', Int)]), Object('Point', {'__repr__': Function(NamedParameters([]), String), 'normalize': Function(NamedParameters([]), Dyn), 'maximize': Function(NamedParameters([('other', TypeVariable('Point'))]), TypeVariable('Point')), 'y': Float, 'x': Float, 'z': Float, })), "\nbm_float11.py:41:0: Function %s does not match specified type Function(['n:Int'], Object(Point, {'__repr__': Function([], String), 'normalize': Function([], Dyn), 'maximize': Function(['other:TypeVar(Point)'], TypeVar(Point)), 'y': Float, 'x': Float, 'z': Float})). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)")
POINTS = 100000

def main(arg, timer):
    times = []
    for i in retic_cast(xrange, Dyn, Function(AnonymousParameters([Int]), Dyn), "\nbm_float11.py:53:13: Expected function of type Function(['Int'], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)")(arg):
        t0 = retic_cast(timer, Dyn, Function(AnonymousParameters([]), Dyn), '\nbm_float11.py:54:13: Expected function of type Function([], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')()
        o = benchmark(POINTS)
        tk = retic_cast(timer, Dyn, Function(AnonymousParameters([]), Dyn), '\nbm_float11.py:56:13: Expected function of type Function([], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')()
        retic_getattr_dynamic(times, 'append', Function(AnonymousParameters([Dyn]), Void))((tk - t0))
    return retic_cast(times, List(Dyn), Dyn, '\nbm_float11.py:58:4: A return value of type Dyn was expected but a value %s was returned instead. (code RETURN_ERROR)')
main = retic_cast(main, Dyn, Function(NamedParameters([('arg', Int), ('timer', Dyn)]), Dyn), "\nbm_float11.py:49:0: Function %s does not match specified type Function(['arg:Int', 'timer:Dyn'], Dyn). Consider changing the type or setting it to Dyn. (code BAD_FUNCTION_INJECTION)")
if (__name__ == '__main__'):
    parser = retic_cast(retic_cast(optparse, Dyn, Object('', {'OptionParser': Dyn, }), '\nbm_float11.py:61:13: Accessing nonexistant object attribute OptionParser from value %s. (code WIDTH_DOWNCAST)').OptionParser, Dyn, Function(DynParameters, Dyn), '\nbm_float11.py:61:13: Expected function of type Function(DynParameters, Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')(usage='%prog [options]', description='Test the performance of the Float benchmark')
    retic_cast(retic_cast(util, Dyn, Object('', {'add_standard_options_to': Dyn, }), '\nbm_float11.py:64:4: Accessing nonexistant object attribute add_standard_options_to from value %s. (code WIDTH_DOWNCAST)').add_standard_options_to, Dyn, Function(AnonymousParameters([Dyn]), Dyn), "\nbm_float11.py:64:4: Expected function of type Function(['Dyn'], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)")(parser)
    (options, args) = retic_cast(retic_cast(retic_cast(parser, Dyn, Object('', {'parse_args': Dyn, }), '\nbm_float11.py:65:20: Accessing nonexistant object attribute parse_args from value %s. (code WIDTH_DOWNCAST)').parse_args, Dyn, Function(AnonymousParameters([]), Dyn), '\nbm_float11.py:65:20: Expected function of type Function([], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')(), Dyn, Tuple(Dyn, Dyn), '\nbm_float11.py:65:4: Right hand side of assignment was expected to be of type Tuple(Dyn,Dyn), but value %s provided instead. (code SINGLE_ASSIGN_ERROR)')
    retic_cast(retic_cast(util, Dyn, Object('', {'run_benchmark': Dyn, }), '\nbm_float11.py:67:4: Accessing nonexistant object attribute run_benchmark from value %s. (code WIDTH_DOWNCAST)').run_benchmark, Dyn, Function(AnonymousParameters([Dyn, Int, Function(NamedParameters([('arg', Int), ('timer', Dyn)]), Dyn)]), Dyn), '\nbm_float11.py:67:4: Expected function of type Function([\'Dyn\', \'Int\', "Function([\'arg:Int\', \'timer:Dyn\'], Dyn)"], Dyn) at call site but but value %s was provided instead. (code FUNC_ERROR)')(options, 1, main)