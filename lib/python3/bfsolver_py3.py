from mazec import Mazec
c = Mazec("test", "kladivko", use_wait=False)

moves = ['sssssssdawwwwwww', 'sssssdawwwww', 'ssssddsdawaawwww', 'ssssddddsdawaaaawwww', 'sssdawww', 'ssddsdawaaww', 'ssddddsdawaaaaww', 'sdaw', 'ddsdawaa', 'ddddsdawaaaa']

def goto(i):
	for x in moves[i]:
		if not c.move(x):
			print(c.error)
	


for i in range(0,10):
	for j in range(0,10):
		goto(i)
		goto(j)
