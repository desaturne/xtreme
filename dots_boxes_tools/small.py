import judge,random,sys
# small boards like the judge's tests 2-7: bot vs oracle, compared with oracle vs oracle
rng=random.Random(int(sys.argv[1])); A=sys.argv[3].split(); O=sys.argv[4].split()
d=[]
for g in range(int(sys.argv[2])):
    while True:
        R,C=rng.choice([(3,3),(4,4),(3,5),(4,5),(4,6),(3,6),(5,5)])
        pos=judge.gen_pos(R,C,rng,rng.choice([0.2,0.4,0.6]))
        if pos and 22<=sum(1 for x in pos[1] if not x)<=44: break
    rows,drawn,sides,eb,NH,E=pos
    f=g%2==0
    p1,s1,_=judge.play(A,O,R,C,rows,drawn,sides,eb,NH,E,f)
    p2,s2,_=judge.play(O,O,R,C,rows,drawn,sides,eb,NH,E,f)
    d.append(p1-p2)
    print(R,C,f,s1,s2,round(p1-p2,2),flush=True)
print("meandiff",sum(d)/len(d),"worse",sum(1 for x in d if x<-1e-6),"better",sum(1 for x in d if x>1e-6))
