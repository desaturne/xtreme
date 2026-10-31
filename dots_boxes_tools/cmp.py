import judge,random,sys
rng=random.Random(int(sys.argv[1])); A=sys.argv[3].split()
d=[];
for g in range(int(sys.argv[2])):
    while True:
        R=rng.randint(1,3); C=rng.randint(1,3)
        pos=judge.gen_pos(R,C,rng,rng.choice([0.3,0.5,0.7]))
        if pos and sum(1 for x in pos[1] if not x)<=22: break
    rows,drawn,sides,eb,NH,E=pos
    f=g%2==0
    p1,s1,_=judge.play(A,["./perfect"],R,C,rows,drawn,sides,eb,NH,E,f)
    p2,s2,_=judge.play(["./perfect"],["./perfect"],R,C,rows,drawn,sides,eb,NH,E,f)
    if abs(p1-p2)>1e-6: print("DIFF",R,C,f,s1,s2,rows)
    d.append(p1-p2)
print("meandiff",sum(d)/len(d),"worse",sum(1 for x in d if x<-1e-6))
