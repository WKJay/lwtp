from building import *

cwd     = GetCurrentDir()
CPPPATH = [cwd]
src     = Glob('*.c')

group = DefineGroup('modules/lwtp', src, depend = [''], CPPPATH = CPPPATH)

Return('group')
