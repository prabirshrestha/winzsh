# makepro.sed by Mark Weaver <Mark_Weaver@brown.edu>
/^\/\*\*\/$/{
n
N
#ifndef WINNT uncomment the next line
#s/\n\([_a-zA-Z][_0-9a-zA-Z]* *\)\((.*\)$/ \1 _(\2);/
#else WINNT uncomment the next line
s/\n\([_a-zA-Z][_0-9a-zA-Z]* *\)(/ \1 _((/
s/$/);/
#endif /* WINNT */
p
}
