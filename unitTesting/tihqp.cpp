/*
 *  Copyright
 */
#define SOTH_DEBUG
#define SOTH_DEBUG_MODE 45
#include "soth/debug.hpp"
#include "soth/HCOD.hpp"
#include "MatrixRnd.hpp"
#include "CODclean.hpp"
#include "RandomGenerator.hpp"

#include <boost/assign/std/vector.hpp> // for 'operator+=()'
using namespace boost::assign; // bring 'operator+=()' into scope

#ifndef WIN32
#include <sys/time.h>
#endif // WIN32

#include <iostream>
#include <sstream>
#include "gettimeofday.hpp"

using namespace soth;
using std::endl;
using std::cout;
using std::cerr;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */


struct NotificationToCout
{
  void operator() (std::string stage,soth::ConstraintRef cst, std::string event)
  {
    std::cout << "At " << stage << ", " << cst << ": " << event << std::endl;
    count ++;
  }
  static int count;
  NotificationToCout()  {count = 0;}
};
int NotificationToCout::count = 0;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void cascadeqp( std::vector<Eigen::MatrixXd> J,
		std::vector<soth::VectorBound> b,
		unsigned int NB_STAGE,
		unsigned int NC )
{
  sotDEBUG(15) << "x = " << (MATLAB)solution << endl;

  HCOD hsolver(NC,NB_STAGE); VectorXd solution(NC);
  NotificationToCout coutListen;
  std::vector<cstref_vector_t> Ir0;

  for( unsigned int i=0;i<NB_STAGE;++i )
    {
      cout << "Stage " << i << endl;
      hsolver.pushBackStage(J[i],b[i]);
      hsolver.notifiorRegistration(coutListen,i);
      hsolver.setNameByOrder("level");
      hsolver.reset();
      if(i==0) hsolver.setInitialActiveSet();
      else
	{
	  sotDEBUG(1) << "Init Ir0" << endl;
	  Ir0.push_back(cstref_vector_t());
	  for( int ii=0;ii<i;++ii )
	    {
	      sotDEBUG(15) << "ii = " << Ir0[ii].size() << endl;
	      for( int cst=0;cst<Ir0[ii].size();++cst )
		{
		  sotDEBUG(15) << ii << Ir0[ii][cst] << endl;
		}
	    }
	  hsolver.setInitialActiveSet(Ir0);
	}
      hsolver.activeSearch(solution);
      cout << "x"<<i << " = " << (MATLAB)solution << endl;
      Ir0 = hsolver.getInitialActiveSet();
    }
  sotDEBUG(1) << "Update Cascade QP = " << coutListen.count << endl;
}

void hqp( std::vector<Eigen::MatrixXd> J,
	  std::vector<soth::VectorBound> b,
	  unsigned int NB_STAGE,
	  unsigned int NC )
{
  HCOD hsolver(NC,NB_STAGE); VectorXd solution(NC);
  for( unsigned int i=0;i<NB_STAGE;++i )
    {
      hsolver.pushBackStage(J[i],b[i]);
      hsolver.setNameByOrder("level");
    }

  NotificationToCout coutListen;
  hsolver.notifiorRegistration(coutListen);
  hsolver.reset();
  hsolver.setInitialActiveSet();
  hsolver.activeSearch(solution);
  sotDEBUG(15) << "x = " << (MATLAB)solution << endl;

  sotDEBUG(1) << "Update HQP = " << coutListen.count << endl;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

#include <boost/program_options.hpp>
#include <Eigen/SVD>

int main (int argc, char** argv)
{
# ifndef NDEBUG
  sotDebugTrace::openFile();
#endif

  unsigned int NB_STAGE,NC;
  std::vector<unsigned int> NR,RANKLINKED,RANKFREE;
  std::vector<Eigen::MatrixXd> J;
  std::vector<soth::VectorBound> b;


  boost::program_options::variables_map optionMap;
  {
    namespace po = boost::program_options;
    po::options_description desc("trandom options");
    desc.add_options()
      ("help", "produce help message")
      ("seed,s",po::value<int>()->default_value(-1),
       "specify the seed of the random generator")
      ("size,S",po::value<int>()->default_value(20),
       "size of the problem to try")
      ("method,m",po::value<int>()->default_value(0),
       "0 = SICILIANO COD, 1 = COD Z, 2 = HCOD")
      ("phase,p",po::value<int>(),
       "Phase of the algo 1 = Decompo, 2 = inversion, 3 = projection")
      ;

    po::positional_options_description p;
    p.add("seed", -1);
    p.add("method", 0);
    p.add("size", 20);

    po::store(po::command_line_parser(argc, argv).
	      options(desc).positional(p).run(), optionMap);
    po::notify(optionMap);

    if (optionMap.count("help"))
      {
	std::cout << desc << std::endl;
	exit(0);
      }
  }

  /* Initialize the seed. */
  {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    int seed = tv.tv_usec % 7919;
    if( optionMap.count("seed") ) { seed = optionMap["seed"].as<int>(); }
    std::cout << "seed = " << seed << std::endl;
    soth::Random::setSeed(seed);
  }

  /* Decide the size of the problem. */
  NB_STAGE = 4; NC = 20;
  NR+=         9,  5,  7,  7;
  RANKFREE +=  6,  3,  3,  6;
  RANKLINKED+= 2,  2,  2,  0;

  std::cout << "NB_STAGE=" << NB_STAGE <<",  NC=" << NC << endl;
  for(int k=0;k<NB_STAGE;k++)
    { sotDEBUG(20) << RANKFREE[k] <<" " <<RANKLINKED[k]<< " " <<NR[k] << endl;}

  /* Initialize J and b. */
  generateDeficientDataSet(J,b,NB_STAGE,RANKFREE,RANKLINKED,NR,NC);

  for( unsigned int i=0;i<NB_STAGE;++i )
     {
# ifndef NDEBUG
      std::cout << "J"<<i+1<<" = " << (soth::MATLAB)J[i] << std::endl;
      std::cout << "b"<<i+1<< " = " << b[i] << ";"<<std::endl;
#endif
    }

# ifndef NDEBUG
    //sotDebugTrace::openFile();
#endif

  int upgradeCascade = 0;
  {
     HCOD hsolver(NC,NB_STAGE); VectorXd solution(NC);
    NotificationToCout coutListen;
    std::vector<cstref_vector_t> Ir0;

    for( unsigned int i=0;i<NB_STAGE;++i )
      {
	cout << "Stage " << i << endl;
	hsolver.pushBackStage(J[i],b[i]);
	hsolver.notifiorRegistration(coutListen,i);
	hsolver.setNameByOrder("level");
	hsolver.reset();
	if(i==0) hsolver.setInitialActiveSet();
	else
	  {
	    sotDEBUG(1) << "Init Ir0" << endl;
	    Ir0.push_back(cstref_vector_t());
	    for( int ii=0;ii<i;++ii )
	      {
		sotDEBUG(15) << "ii = " << Ir0[ii].size() << endl;
		for( int cst=0;cst<Ir0[ii].size();++cst )
		  {
		    sotDEBUG(15) << ii << Ir0[ii][cst] << endl;
		  }
	      }
	    hsolver.setInitialActiveSet(Ir0);
	  }
	hsolver.activeSearch(solution);
	cout << "x"<<i << " = " << (MATLAB)solution << endl;
	Ir0 = hsolver.getInitialActiveSet();
      }
    upgradeCascade = coutListen.count;
  }

  {
    HCOD hsolver(NC,NB_STAGE); VectorXd solution(NC);
    for( unsigned int i=0;i<NB_STAGE;++i )
      {
	hsolver.pushBackStage(J[i],b[i]);
	hsolver.setNameByOrder("level");
      }
    NotificationToCout coutListen;
    hsolver.notifiorRegistration(coutListen);
    hsolver.reset();
    hsolver.setInitialActiveSet();
    hsolver.activeSearch(solution);
    cout << "x = " << (MATLAB)solution << endl;

    cout << endl << endl << "Cascade = " << upgradeCascade
	 << "   HQP = " << coutListen.count << endl;
  }


  
# ifndef NDEBUG
  //exit(0);
#endif

  /* --------------------------------------------------------------------- */
  /* --------------------------------------------------------------------- */
  /* --------------------------------------------------------------------- */
  struct timeval t0,t1;
  double totalTime = 0;
  std::ostringstream iss;
  cout <<optionMap["size"].as<int>()<<endl;
  cout<<optionMap["method"].as<int>()<<endl;
  iss <<"/tmp/ihqp_";
    //<<optionMap["size"].as<int>()<<"_"
    //<<optionMap["method"].as<int>()<<".dat";
  std::ofstream datfile(iss.str().c_str());

  for( int shoot=0;shoot<1000;shoot++ )
    {
      double seed = Random::rand<int>();// % 704819;
      soth::Random::setSeed(seed);
      std::cout << "s"<<seed << ":" << std::flush;

      generateFixedSizeRandomProfile(optionMap["size"].as<int>(),
				     1.5,0.8,0.5,NB_STAGE,RANKFREE,RANKLINKED,NR,NC);
      generateDeficientDataSet(J,b,NB_STAGE,RANKFREE,RANKLINKED,NR,NC);

      gettimeofday(&t0,NULL);

      switch(optionMap["method"].as<int>())
	{
	case 0:
	  break;
	}

      gettimeofday(&t1,NULL);
      double time = (t1.tv_sec-t0.tv_sec)+(t1.tv_usec-t0.tv_usec)/1.0e6;
      totalTime += time;
      
      datfile << NB_STAGE << "        \t " << time <<  "\t" << seed << std::endl;
    }
  std::cout << totalTime << std::endl;


}
