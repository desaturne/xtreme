import judge,sys
cases=[(4,4,False,['+-+-+.+.+', '.o.o.o|o.', '+.+.+-+.+', '.o.o.o|o|', '+.+.+.+.+', '|o.o.o.o.', '+.+-+.+.+', '|o.o.o|o.', '+.+-+-+-+']),
(4,4,True,['+-+-+.+.+', '.o.o.o|o.', '+-+-+-+.+', '|o.o.o.o|', '+.+-+-+-+', '|o|#|o.o|', '+.+-+.+.+', '.o|o.o|o|', '+-+.+.+.+']),
(3,5,False,['+-+-+.+-+.+', '.o|#|o.o|o.', '+.+-+.+.+-+', '|o.o|o|o.o.', '+-+.+.+.+.+', '.o.o|o|o|o.', '+.+-+.+.+-+'])]
A=sys.argv[1].split()
def parse(R,C,rows):
    NH=(R+1)*C; E=NH+R*(C+1); eb=[[-1,-1] for _ in range(E)]
    for r in range(R):
        for c in range(C):
            b=r*C+c
            for e in [r*C+c,(r+1)*C+c,NH+r*(C+1)+c,NH+r*(C+1)+c+1]:
                if eb[e][0]<0: eb[e][0]=b
                else: eb[e][1]=b
    drawn=[False]*E
    for r in range(R+1):
        for c in range(C):
            drawn[r*C+c]=rows[2*r][2*c+1]=='-'
    for r in range(R):
        for c in range(C+1):
            drawn[NH+r*(C+1)+c]=rows[2*r+1][2*c]=='|'
    sides=[sum(drawn[e] for e in range(E) if b in eb[e]) for b in range(R*C)]
    return drawn,sides,eb,NH,E
for R,C,f,rows in cases:
    drawn,sides,eb,NH,E=parse(R,C,rows)
    res=[]
    for k in range(3):
        p1,s1,_=judge.play(A,["./perfect"],R,C,rows,drawn,sides,eb,NH,E,f); res.append(s1)
    print(R,C,f,res)
