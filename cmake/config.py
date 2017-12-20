from sysconfig import get_paths
from pprint import pprint
import sys

info = get_paths()
ae = len(sys.argv) 
ai = 1
while ai < ae:
	print("%s" % info[ sys.argv[ ai]])
	ai += 1


