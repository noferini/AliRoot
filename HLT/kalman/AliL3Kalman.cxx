#include "AliL3StandardIncludes.h"
#include <sys/time.h>

#include "AliL3Benchmark.h"
#ifdef use_aliroot
#include "AliL3FileHandler.h"
#else
#include "AliL3MemHandler.h"
#endif
#include "AliL3DataHandler.h"
#include "AliL3Transform.h"
#include "AliL3SpacePointData.h"
#include "AliL3DigitData.h"
#include "AliL3Kalman.h"
#include "AliL3Logging.h"
#include "AliL3TrackArray.h"
#include "AliL3Track.h"
#include "AliL3KalmanTrack.h"
#include "AliL3TrackSegmentData.h"
#include "AliL3InterMerger.h"
#include "AliL3TrackMerger.h"

#include "TNtuple.h"
#include "TTimer.h"

/*
  AliL3Kalman
*/

ClassImp(AliL3Kalman)

AliL3Kalman::AliL3Kalman(Char_t *datapath, Int_t *slice, Int_t min_clusters = 0){
  // Constructor
  if (slice)
    {
      fMinSlice = slice[0];
      fMaxSlice = slice[1];
    }
  else
    {
      fMinSlice = 0;
      fMaxSlice = 35;
    }
  
  sprintf(fPath,"%s",datapath);
  fMinPointsOnTrack = 0; //min_clusters;
  fTracks = 0;
  fKalmanTracks = 0;

  // NB! fNrow under only for single-patch, must include other possibilities 
  // later on. ?? Maybe better also to put it in an Init-function
  fRow[0][0] = 0;
  fRow[0][1] = AliL3Transform::GetLastRow(-1);
  fWriteOut = kTRUE;
  fBenchmark = 0;

  fMakeSeed = kFALSE;
}  

AliL3Kalman::~AliL3Kalman()
{
  // Destructor
  if (fBenchmark) delete fBenchmark;
}

void AliL3Kalman::Init()
{
  fBenchmark = new AliL3Benchmark();
}

void AliL3Kalman::LoadTracks(Int_t event, Bool_t sp)
{
  // Load space points and tracks from conformal tracker
  // Must also be possible to take seeds (and clusters) from Hough-transform??

  Double_t initTime,cpuTime;
  initTime = GetCpuTime();
  fBenchmark->Start("Load tracks");

  // Load space points
  Char_t fname[1024];
  AliL3FileHandler *clusterfile[36][6];
  for(Int_t s=fMinSlice; s<=fMaxSlice; s++)
    {
      for(Int_t p=0; p<AliL3Transform::GetNPatches(); p++)
        {
          Int_t patch;
          if(sp==kTRUE)
            patch=-1;
          else
            patch=p;
	  
          fClusters[s][p] = 0;
          clusterfile[s][p] = new AliL3FileHandler();
          if(event<0)
            sprintf(fname,"%s/points_%d_%d.raw",fPath,s,patch);
          else
            sprintf(fname,"%s/points_%d_%d_%d.raw",fPath,event,s,patch);
          if(!clusterfile[s][p]->SetBinaryInput(fname))
            {
              LOG(AliL3Log::kError,"AliL3Evaluation::Setup","File Open")
                <<"Inputfile "<<fname<<" does not exist"<<ENDLOG;
              delete clusterfile[s][p];
              clusterfile[s][p] = 0;
              continue;
            }
          fClusters[s][p] = (AliL3SpacePointData*)clusterfile[s][p]->Allocate();
          clusterfile[s][p]->Binary2Memory(fNcl[s][p],fClusters[s][p]);
          clusterfile[s][p]->CloseBinaryInput();
	  if(sp==kTRUE)
            break;
	  delete clusterfile[s][p];
        }
    }

  // Load tracks
  sprintf(fname,"%s/tracks_%d.raw",fPath,event);
  AliL3FileHandler *tfile = new AliL3FileHandler();
  if(!tfile->SetBinaryInput(fname)){
    LOG(AliL3Log::kError,"AliL3Evaluation::Setup","File Open")
      <<"Inputfile "<<fname<<" does not exist"<<ENDLOG;
    return;
  }
  if(fTracks)
    delete fTracks;
  fTracks = new AliL3TrackArray();
  tfile->Binary2TrackArray(fTracks);
  tfile->CloseBinaryInput();
  delete tfile;
  fBenchmark->Stop("Load tracks");
  cpuTime = GetCpuTime() - initTime;
  LOG(AliL3Log::kInformational,"AliL3Kalman::LoadTracks()","Timing")
    <<"Loading tracks in "<<cpuTime*1000<<" ms"<<ENDLOG;

}

void AliL3Kalman::ProcessTracks()
{
  // Run the Kalman filter algorithm on the loaded tracks. 
  // If the track is OK, the loaded track is saved in file kalmantracks_0.raw
  // The kalman filter variables (that is the state vector, covariance matrix 
  // and chi2) is written to root-file kalmantracks.root. 
  // Should be extended to correct the track parameters of the loaded track
  // or save the kalmantrack instead of the loaded track.??
  UInt_t fEvent = 0;

  Double_t initTime,cpuTime;
  initTime = GetCpuTime();

  fBenchmark->Start("Process tracks");

  fTracks->QSort();

  fKalmanTracks = new AliL3TrackArray();

  // Make a tree to store state vector, covariance matrix and chisquare
  // Will eventually not need a TTree??
  TTree *kalmanTree = new TTree("kalmanTree","kalmantracks");
  Float_t meas[21];
  Int_t lab = 123456789;
  kalmanTree->Branch("x0",&meas[0],"x0/F");
  kalmanTree->Branch("x1",&meas[1],"x1/F");
  kalmanTree->Branch("x2",&meas[2],"x2/F");
  kalmanTree->Branch("x3",&meas[3],"x3/F");
  kalmanTree->Branch("x4",&meas[4],"x4/F");
  kalmanTree->Branch("c0",&meas[5],"c0/F");
  kalmanTree->Branch("c1",&meas[6],"c1/F");
  kalmanTree->Branch("c2",&meas[7],"c2/F");
  kalmanTree->Branch("c3",&meas[8],"c3/F");
  kalmanTree->Branch("c4",&meas[9],"c4/F");
  kalmanTree->Branch("c5",&meas[10],"c5/F");
  kalmanTree->Branch("c6",&meas[11],"c6/F");
  kalmanTree->Branch("c7",&meas[12],"c7/F");
  kalmanTree->Branch("c8",&meas[13],"c8/F");
  kalmanTree->Branch("c9",&meas[14],"c9/F");
  kalmanTree->Branch("c10",&meas[15],"c10/F");
  kalmanTree->Branch("c11",&meas[16],"c11/F");
  kalmanTree->Branch("c12",&meas[17],"c12/F");
  kalmanTree->Branch("c13",&meas[18],"c13/F");
  kalmanTree->Branch("c14",&meas[19],"c14/F");
  kalmanTree->Branch("chisq",&meas[20],"chisq/F");
  kalmanTree->Branch("lab",&lab,"lab/I");

  // Go through the tracks from conformal or hough tracker
  for (Int_t iTrack = 0; iTrack < fTracks->GetNTracks(); iTrack++)
    {
      /*Double_t initTime,cpuTime;
      initTime = GetCpuTime();
      fBenchmark->Start("Process tracks");*/

      AliL3Track *track = (AliL3Track*)fTracks->GetCheckedTrack(iTrack);
      if (!track) continue;
      if (track->GetNumberOfPoints() < fMinPointsOnTrack) continue;    

      UInt_t *hitnum = track->GetHitNumbers();
      UInt_t id;
      
      id = hitnum[0];
      Int_t slice = (id>>25) & 0x7f;
      Int_t patch = (id>>22) & 0x7;	
      UInt_t pos = id&0x3fffff;
      AliL3SpacePointData *points = fClusters[slice][patch];
      lab=points[pos].fTrackID[0];
      //cout << lab << endl;

      AliL3KalmanTrack *kalmantrack = new AliL3KalmanTrack();

      Bool_t save = kTRUE;
      if (fMakeSeed == kTRUE)
	{
	  if (MakeSeed(kalmantrack, track) == 0)
	    {
	      save = kFALSE;
	      continue;
	    }
	}
      else 
	{
	  if (InitKalmanTrack(kalmantrack, track) == 0)
	    {
	      save = kFALSE;
	      continue;
	    }
	}

      if (Propagate(kalmantrack, track) == 0) 
	{
	  save = kFALSE;
	  continue;
	}
      
      if (save) {
	Float_t x[5]; 
	kalmantrack->GetStateVector(x);
	Float_t c[15]; 
	kalmantrack->GetCovariance(c);
	Float_t chisq = kalmantrack->GetChisq();
	meas[0]  = x[0];
	meas[1]  = x[1];
	meas[2]  = x[2];
	meas[3]  = x[3];
	meas[4]  = x[4];
	meas[5]  = c[0];
	meas[6]  = c[1];
	meas[7]  = c[2];
	meas[8]  = c[3];
	meas[9]  = c[4];
	meas[10] = c[5];
	meas[11] = c[6];
	meas[12] = c[7];
	meas[13] = c[8];
	meas[14] = c[9];
	meas[15] = c[10];
	meas[16] = c[11];
	meas[17] = c[12];
	meas[18] = c[13];
	meas[19] = c[14];
	meas[20] = chisq;

	// Add the track to the trackarray	
	AliL3Track *outtrack = (AliL3Track*)fKalmanTracks->NextTrack();
	outtrack->Set(track);

	// Fill the ntuple with the state vector, covariance matrix and
	// chisquare and track label
	kalmanTree->Fill();
      }

      delete track;
      delete kalmantrack;

      /*fBenchmark->Stop("Process tracks");
      cpuTime = GetCpuTime() - initTime;
      LOG(AliL3Log::kInformational,"AliL3Kalman::ProcessTracks()","Timing")
      <<"Processed track "<<iTrack<<" in "<<cpuTime*1000<<" ms"<<ENDLOG;*/
    }

  fBenchmark->Stop("Process tracks");
  cpuTime = GetCpuTime() - initTime;
  LOG(AliL3Log::kInformational,"AliL3Kalman::ProcessTracks()","Timing")
    <<"Process tracks in "<<cpuTime*1000<<" ms"<<ENDLOG;
  
  if (fWriteOut)
    {
      Char_t tname[80];
      sprintf(tname,"%s/kalmantracks_%d.raw",fWriteOutPath,fEvent);
      AliL3MemHandler *mem = new AliL3MemHandler();
      mem->SetBinaryOutput(tname);
      mem->TrackArray2Binary(fKalmanTracks);
      mem->CloseBinaryOutput();
      delete mem;
    }
  
  // This will be removed??
  TFile *out = new TFile("kalmantracks.root","recreate");      
  kalmanTree->Write();
  out->Close();

  delete kalmanTree;
  
}

Int_t AliL3Kalman::InitKalmanTrack(AliL3KalmanTrack *kalmantrack, AliL3Track *track)
{
  UInt_t *hitnum = track->GetHitNumbers();
  UInt_t id;

  id = hitnum[0];
  Int_t slice = (id>>25) & 0x7f;
  Int_t patch = (id>>22) & 0x7;	
  UInt_t pos = id&0x3fffff;
  AliL3SpacePointData *points = fClusters[slice][patch];

  return kalmantrack->Init(track, points, pos);
}

Int_t AliL3Kalman::MakeSeed(AliL3KalmanTrack *kalmantrack, AliL3Track *track)
{  
  // Makes a rough state vector and covariance matrix based on three
  // space points of the loaded track 
  // Will eventually be removed?? Will use track parameters from conformal
  // tracker or HT as seeds for the kalman filter.
 
  UInt_t *hitnum = track->GetHitNumbers();
  UInt_t id1, id2, id3;

  id1 = hitnum[0];
  Int_t slice1 = (id1>>25) & 0x7f;
  Int_t patch1 = (id1>>22) & 0x7;	
  UInt_t pos1 = id1&0x3fffff;
  AliL3SpacePointData *points1 = fClusters[slice1][patch1];
  
  id2 = hitnum[Int_t(track->GetNHits()/2)];
  Int_t slice2 = (id2>>25) & 0x7f;
  Int_t patch2 = (id2>>22) & 0x7;	
  UInt_t pos2 = id2&0x3fffff;
  AliL3SpacePointData *points2 = fClusters[slice2][patch2];

  id3 = hitnum[track->GetNHits()-1];
  Int_t slice3 = (id3>>25) & 0x7f;
  Int_t patch3 = (id3>>22) & 0x7;	
  UInt_t pos3 = id3&0x3fffff;
  AliL3SpacePointData *points3 = fClusters[slice3][patch3];

  return kalmantrack->MakeTrackSeed(points1,pos1,points2,pos2,points3,pos3);

}

Int_t AliL3Kalman::Propagate(AliL3KalmanTrack *kalmantrack, AliL3Track *track)
{
  // This function propagtes the kalmantrack to the next cluster of the loaded 
  // track 
  Int_t num_of_clusters = track->GetNumberOfPoints();
  UInt_t *hitnum = track->GetHitNumbers();
  UInt_t id;
  UInt_t badpoint = 0;

  for (Int_t icl = 1; icl < num_of_clusters; icl++)
    {

      id = hitnum[icl];
      Int_t slice = (id>>25) & 0x7f;
      Int_t patch = (id>>22) & 0x7;
      UInt_t pos = id&0x3fffff;

      AliL3SpacePointData *points = fClusters[slice][patch];
      if (!points) continue;
      if (kalmantrack->Propagate(points,pos) == 0) 
	{
	  badpoint++;
	  continue;
	}
    }
  
  // If too many clusters are missing, the track is no good
  if (badpoint >= UInt_t(num_of_clusters*0.8)) return 0;
  return 1;
}

Double_t AliL3Kalman::GetCpuTime()
{
  //Return the Cputime in seconds.
 struct timeval tv;
 gettimeofday( &tv, NULL );
 return tv.tv_sec+(((Double_t)tv.tv_usec)/1000000.);
 //return (Double_t)(clock()) / CLOCKS_PER_SEC;
}
