#include "iqcombiner.h"

using namespace std;
using boost::format;


IQCombiner::
IQCombiner() 
    : initted(false), 
      do_condense(false) 
    { }

IQCombiner::
IQCombiner(
    const IQIndex& l1, const IQIndex& l2, 
    const IQIndex& l3, const IQIndex& l4, 
    const IQIndex& l5, const IQIndex& l6)
    : initted(false), 
      do_condense(false)
    {
    if(l1 == IQIndex::Null()) Error("Null IQIndex");
    if(l1 != IQIndex::Null()) left_.push_back(l1); 
    if(l2 != IQIndex::Null()) left_.push_back(l2);
    if(l3 != IQIndex::Null()) left_.push_back(l3); 
    if(l4 != IQIndex::Null()) left_.push_back(l4);
    if(l5 != IQIndex::Null()) left_.push_back(l5); 
    if(l6 != IQIndex::Null()) left_.push_back(l6);
    Foreach(IQIndex& L, left_) L.conj();
    }


void IQCombiner::
doCondense(bool val)
    {
    if(initted) 
        Error("Can't set doCondense after already initted.");
    do_condense = val;
    }

 
void IQCombiner::
reset()
    {
    left_.clear();
    initted = false;
    }


void IQCombiner::
addleft(const IQIndex& l) 	// Include another left index
	{ 
    if(l == IQIndex::Null()) Error("Null IQIndex");
    left_.push_back(l);
    //Flip arrows to make combiner compatible with
    //the IQTensor from which it got its left indices
    left_.back().conj();
    initted = false;
	}


void IQCombiner::
init(string rname, IndexType type, 
     Arrow dir, int primelevel) const 
    {
    if(initted) return;
    if(left_.size() == 0)
        Error("No left indices in IQCombiner.");

    Arrow rdir = dir; 
    if(dir == Neither) //determine automatically
        {
        //Prefer to derive right Arrow from Link indices
        Foreach(const IQIndex& J, left_)
            {
            rdir = -J.dir(); 
            break;
            }
        }

    if(rdir == Neither)
        {
        cout << "left_ = " << endl;
        Foreach(const IQIndex& J, left_)
            cout << J << "\n" << endl;
        Error("Failed to determine right IQIndex direction");
        }

    //Construct individual Combiners
    QCounter c(left_);
    IQIndex::Storage iq;
    for( ; c.notDone(); ++c)
        {
        vector<Index> vind;
        QN q;
        c.getVecInd(left_,vind, q); // updates vind and q
        q *= -rdir;

        combs.push_back(Combiner());
        Combiner& co = combs.back();
        co.addleft(vind);
        co.init(rname+q.toString(),type,rdir,primelevel);

        iq.push_back(IndexQN(co.right(),q));
        }
    if(do_condense) 
        {
        ucright_ = IQIndex(rname,iq,rdir,primelevel);
        string cname = "cond::" + rname;
        cond = Condenser(ucright_,cname);
        right_ = cond.smallind();
        }
    else 
        {
        right_ = IQIndex(rname,iq,rdir,primelevel);
        }
    initted = true;
	}

IQCombiner::
operator IQTensor() const
    {
    if(!initted) Error("IQCombiner::operator IQTensor(): IQCombiner not initialized.");

    vector<IQIndex> iqinds(left_);
    iqinds.push_back((do_condense ? ucright_ : right_));
    IQTensor res(iqinds);

    Foreach(const Combiner& co, combs)
        {
        //Here we are using the fact that Combiners
        //can be converted to ITensors
        res.insert(co);
        }

#ifdef DEBUG
    //Combiners should always have the 
    //structure of zero divergence IQTensors
    const QN Zero;
    if(div(res) != Zero)
        {
        Print(res);
        Error("IQTensor divergence not zero");
        }
#endif

    if(do_condense) 
        { 
        IQTensor rcopy(res); 
        cond.product(rcopy,res); 
        }

    return res;
    }


const IQIndex& IQCombiner::
right() const 
    { 
    init();
    return right_;
    }

void  IQCombiner::
prime(IndexType type, int inc)
    {
    Foreach(IQIndex& ll, left_)
        ll.prime(type,inc);
    Foreach(Combiner& co, combs)
        co.prime(type,inc);
    if(initted)
        {
        right_.prime(type,inc);
        if(do_condense) 
            {
            cond.prime(type,inc);
            ucright_.prime(type,inc);
            }
        }
    }

IQCombiner 
primed(IQCombiner C, int inc)
    {
    C.prime(All,inc);
    return C;
    }



void IQCombiner::
conj() 
    { 
    init();
    Foreach(IQIndex& I, left_) I.conj(); 
    if(do_condense) 
        {
        cond.conj();
        ucright_.conj();
        }
    right_.conj();
    }

void IQCombiner::
product(IQTensor T, IQTensor& res) const
    {
    init();
    vector<IQIndex> iqinds;

    if(hasindex(T,right_))
        {
        //
        //T has right IQIndex, expand it
        //
        IQTensor T_uncondensed;
        if(do_condense) 
            { 
            cond.product(T,T_uncondensed); 
            }
        const IQTensor& T_ = (do_condense ? T_uncondensed : T);
        const IQIndex& r = (do_condense ? ucright_ : right_);

        if(Global::checkArrows())
            if(dir(T_.indices(),r) == r.dir())
                {
                cout << "IQTensor = " << T_ << endl;
                cout << "IQCombiner = " << *this << endl;
                cout << "(Right) IQIndex from IQCombiner = " << r << endl;
                Error("Incompatible arrow directions in operator*(IQTensor,IQCombiner).");
                }

        iqinds.reserve(T_.indices().r()-1+left_.size());

        Foreach(const IQIndex& I, T_.indices())
            {
            if(I == r)
                copy(left_.begin(),left_.end(),back_inserter(iqinds));
            else
                iqinds.push_back(I);
            }

        res = IQTensor(iqinds);

        map<Index, const Combiner*> rightcomb;
        Foreach(const Combiner& co, combs)
            {
            rightcomb[co.right()] = &co;
            }

        Foreach(const ITensor& tt, T_.blocks())
        Foreach(const Index& K, tt.indices())
            {
            if(hasindex(r,K))
                { 
                res += (*(rightcomb[K]) * tt); 
                break;
                }
            } //end for

        }
    else
        {
        //
        //T has left IQIndex's, combine them
        //

        iqinds.reserve(T.r()-left_.size()+1);

        //res will have all IQIndex's of T not in the left of c
        Foreach(const IQIndex& I, T.indices()) 
            { 
            if(!hasindex(*this,I)) iqinds.push_back(I); 
            }
        //and res will have c's right IQIndex
        if(do_condense) iqinds.push_back(ucright_);
        else            iqinds.push_back(right_);

        res = IQTensor(iqinds);

        //Check left indices
        Foreach(const IQIndex& I, left_)
            {
            if(!hasindex(T,I))
                {
                cout << "Could not find left IQIndex " << I << "\n";
                Print(T.indices());
                cout << "Left indices\n";
                for(size_t j = 0; j < left_.size(); ++j)
                    { 
                    cout << j SP left_[j] << "\n"; 
                    }
                Error("bad IQCombiner IQTensor product");
                }
            else //IQIndex is in left
                {
                //Check arrow directions
                if(Global::checkArrows())
                    if(dir(T.indices(),I) == I.dir())
                        {
                        Print(T.indices());
                        Print((*this));
                        cout << "(Left) IQIndex from IQCombiner = " << I << endl;
                        Error("Incompatible arrow directions in operator*(IQTensor,IQCombiner).");
                        }
                }
            }

        //Create map of Combiners using uniqueReal as key
        map<ApproxReal, const Combiner*> combmap;
        Foreach(const Combiner& co, combs)
            {
            combmap[co.uniqueReal()] = &co;
            }

        //Loop over each block in T and apply appropriate
        //Combiner (determined by the uniqueReal of the 
        //combined Indices)
        Foreach(const ITensor& t, T.blocks())
            {
            Real block_ur = 0;
            Foreach(const Index& K, t.indices())
                {
                if(hasindex(*this,K)) 
                    block_ur += K.uniqueReal();
                }

            if(combmap.count(block_ur) == 0)
                {
                Print(t);
                cout << "\nleft indices \n";
                for(size_t j = 0; j < left_.size(); ++j)
                    { cout << j << " " << left_[j] << "\n"; }
                cout << "\n" << endl;

                typedef map<ApproxReal, const Combiner*>::const_iterator
                combmap_const_it;
                for(combmap_const_it uu = combmap.begin();
                    uu != combmap.end(); ++uu)
                    {
                    cout << "Combiner: " << endl;
                    cout << *(uu->second) << endl;
                    }
                Error("no combmap entry for block_ur in IQCombiner prod");
                }

            res += (*combmap[block_ur] * t);
            }

        if(do_condense) 
            { 
            IQTensor rcopy(res); 
            cond.product(rcopy,res); 
            }
        }
    } //void product(const IQTensor& T, IQTensor& res) const

bool
hasindex(const IQCombiner& C, const IQIndex& I)
	{
    Foreach(const IQIndex& J, C.left())
        if(J == I) return true;
    return false;
	}

bool
hasindex(const IQCombiner& C, const Index& i)
    {
    Foreach(const IQIndex& J, C.left())
        if(hasindex(J,i)) return true;
    return false;
    }

ostream& 
operator<<(ostream & s, const IQCombiner & c)
    {
    if(c.isInit())
        { s << endl << "Right index is " << c.right() << "\n"; }
    else
        { s << endl << "Right index is not initialized\n\n"; }
    s << "Left indices: \n";
    Foreach(const IQIndex& I, c.left_) s << I << endl;
    return s << "\n\n";
    }
