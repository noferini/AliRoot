// $Id: AliHLTCaloClusterizerComponent.cxx 36709 2009-11-12 16:57:55Z odjuvsla $

/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Authors: Oystein Djuvsland <oysteind@ift.uib.no>                       *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

#include "AliHLTCaloClusterizerComponent.h"
#include "AliHLTCaloClusterizer.h"
#include "AliHLTCaloClusterAnalyser.h" 
#include "AliHLTCaloRecPointDataStruct.h"
#include "AliHLTCaloRecPointHeaderStruct.h"
#include "AliHLTCaloDigitDataStruct.h"
#include "AliHLTCaloDigitContainerDataStruct.h"
#include "AliHLTCaloDefinitions.h"
#include "AliHLTCaloClusterDataStruct.h"
#include "TString.h"

/** @file   AliHLTCaloClusterizerComponent.cxx
    @author Oystein Djuvsland
    @date   
    @brief  A clusterizer component for PHOS HLT
*/

// see header file for class documentation
// or
// refer to README to build package
// or
// visit http://web.ift.uib.no/~kjeks/doc/alice-hlt

AliHLTCaloClusterizerComponent::AliHLTCaloClusterizerComponent(TString det): 
  AliHLTCaloProcessor(),
  AliHLTCaloConstantsHandler(det),
  fDigitsPointerArray(0), 
  fClusterizerPtr(0),
  fAnalyserPtr(0),
  fDigitCount(0)
{
  //See headerfile for documentation

  fDigitsPointerArray = new AliHLTCaloDigitDataStruct*[fCaloConstants->GetNXCOLUMNSRCU()*fCaloConstants->GetNZROWSRCU()];

  fClusterizerPtr = new AliHLTCaloClusterizer(det);

  fClusterizerPtr->SetDigitArray(fDigitsPointerArray);
   
  fAnalyserPtr = new AliHLTCaloClusterAnalyser();
  
}

AliHLTCaloClusterizerComponent::~AliHLTCaloClusterizerComponent()
{
  //See headerfile for documentation
delete fAnalyserPtr;
  if(fClusterizerPtr)
    {
      delete fClusterizerPtr;
      fClusterizerPtr = 0;
    }
}


int
AliHLTCaloClusterizerComponent::Deinit()
{
  //See headerfile for documentation

if (fClusterizerPtr)
    {
      delete fClusterizerPtr;
      fClusterizerPtr = 0;
    }

  return 0;
}

// void
// AliHLTCaloClusterizerComponent::GetInputDataTypes( vector<AliHLTComponentDataType>& list)
// {
//   //See headerfile for documentation
//   list.clear();
//   list.push_back(AliHLTCaloDefinitions::fgkDigitDataType|kAliHLTDataOriginPHOS);
// }

// AliHLTComponentDataType
// AliHLTCaloClusterizerComponent::GetOutputDataType()
// {
//   //See headerfile for documentation
//   return AliHLTCaloDefinitions::fgkRecPointDataType|kAliHLTDataOriginPHOS;
// }

// void
// AliHLTCaloClusterizerComponent::GetOutputDataSize(unsigned long& constBase, double& inputMultiplier )

// {
//   //See headerfile for documentation
//   constBase = sizeof(AliHLTCaloRecPointHeaderStruct) + sizeof(AliHLTCaloRecPointDataStruct) + (sizeof(AliHLTCaloDigitDataStruct) << 7); //Reasonable estimate... ;
//   inputMultiplier = 1.5;
// }

int
AliHLTCaloClusterizerComponent::DoEvent(const AliHLTComponentEventData& evtData, const AliHLTComponentBlockData* blocks,
                                        AliHLTComponentTriggerData& /*trigData*/, AliHLTUInt8_t* outputPtr, AliHLTUInt32_t& size,
                                        std::vector<AliHLTComponentBlockData>& outputBlocks)
{
  //See headerfile for documentation

  if(blocks == 0) return 0;
  
  UInt_t offset           = 0;
  UInt_t mysize           = 0;
  Int_t nRecPoints        = 0;
  Int_t nDigits           = 0;
  Int_t digCount          = 0;

  UInt_t availableSize = size;
  AliHLTUInt8_t* outBPtr;
  outBPtr = outputPtr;
  const AliHLTComponentBlockData* iter = 0;
  unsigned long ndx;
  
  UInt_t specification = 0;
  
  AliHLTCaloDigitDataStruct *digitDataPtr = 0;
  
//   delete fAnalyserPtr;
//   fAnalyserPtr  = new AliHLTCaloClusterAnalyser();
//   
  // Adding together all the digits, should be put in standalone method  
  for ( ndx = 0; ndx < evtData.fBlockCnt; ndx++ )
    {
      iter = blocks+ndx;
      //            HLTError("Got block");
      if (iter->fDataType == (AliHLTCaloDefinitions::fgkDigitDataType|fDataOrigin))
	{

//	  fDigitsPointerArray[digCount] = reinterpret_cast<AliHLTCaloDigitDataStruct*>(iter->fPtr);
	  
	  //cout << "Digits pointer array [" << digCount << "]: " << fDigitsPointerArray[digCount] << endl;
	  // Update the number of digits
	  nDigits = iter->fSize/sizeof(AliHLTCaloDigitDataStruct);;
//	  digCount += nDigits;
	  //	  HLTError("Got %d digits", nDigits);
	  
	  availableSize -= iter->fSize;
	  
	  specification = specification|iter->fSpecification;

   	  digitDataPtr = reinterpret_cast<AliHLTCaloDigitDataStruct*>(iter->fPtr);
   	  for (Int_t i = 0; i < nDigits; i++)
   	    {
   	      fDigitsPointerArray[digCount] = digitDataPtr;
   	      digCount++;
   	      digitDataPtr++;
   	    }
	}
    }

  if(digCount > 0)
    {
       
      AliHLTCaloClusterHeaderStruct* caloClusterHeaderPtr = reinterpret_cast<AliHLTCaloClusterHeaderStruct*>(outBPtr);
      caloClusterHeaderPtr->fNDigits = digCount;
      
      outBPtr += sizeof(AliHLTCaloClusterHeaderStruct);
      mysize += sizeof(AliHLTCaloClusterHeaderStruct);
      
      // Sort the digit pointers
      qsort(fDigitsPointerArray, digCount, sizeof(AliHLTCaloDigitDataStruct*), CompareDigits);

      // Copy the digits to the output
      fOutputDigitsArray = reinterpret_cast<AliHLTCaloDigitDataStruct*>(outBPtr);
      for(Int_t n = 0; n < digCount; n++)
	{
	  memcpy(outBPtr, fDigitsPointerArray[n], sizeof(AliHLTCaloDigitDataStruct));
	  //fOutputDigitsArray[n] = reinterpret_cast<AliHLTCaloDigitDataStruct*>(outBPtr);
	  outBPtr = outBPtr + sizeof(AliHLTCaloDigitDataStruct);
	  printf("CL: digit energy: %f, ID: %d\n", fOutputDigitsArray[n].fEnergy, fOutputDigitsArray[n].fID);
	}
  
       mysize += digCount*sizeof(AliHLTCaloDigitDataStruct);

//       AliHLTComponentBlockData bdDigits;
//       FillBlockData( bdDigits );
//       bdDigits.fOffset = offset;
//       bdDigits.fSize = mysize;
//       bdDigits.fDataType = iter->fDataType;
//       bdDigits.fSpecification = specification;
//       outputBlocks.push_back( bdDigits );

      //HLTDebug("Total number of digits: %d", digCount );

      nRecPoints = fClusterizerPtr->ClusterizeEvent(digCount);
      //fClusterizerPtr->CheckDigits();
      //HLTDebug("Number of rec points found: %d", nRecPoints);

      fAnalyserPtr->SetCaloClusterData(reinterpret_cast<AliHLTCaloClusterDataStruct*>(outBPtr));
      
      //fClusterizerPtr->CheckDigits(fClusterizerPtr->GetRecPoints(), fOutputDigitsArray, nRecPoints);

      fAnalyserPtr->SetRecPointArray(fClusterizerPtr->GetRecPoints(), nRecPoints);

      //fClusterizerPtr->CheckDigits(fClusterizerPtr->GetRecPoints(), fOutputDigitsArray, nRecPoints);

      fAnalyserPtr->SetDigitDataArray(fOutputDigitsArray);

//      fClusterizerPtr->CheckDigits(fClusterizerPtr->GetRecPoints(), fOutputDigitsArray, nRecPoints);

      Int_t nClusters = fAnalyserPtr->CreateClusters(nRecPoints, size, mysize);
  
      caloClusterHeaderPtr->fNClusters = nClusters;
      
      //  HLTError("Number of clusters: %d", nRecPoints);

      
      
      AliHLTComponentBlockData bd;
      FillBlockData( bd );
      bd.fOffset = offset;
      bd.fSize = mysize;
      bd.fDataType = kAliHLTDataTypeCaloCluster | kAliHLTDataOriginPHOS;
      bd.fSpecification = specification;
      outputBlocks.push_back( bd );
    }

  size = mysize;
  
  return 0;
}

int 
AliHLTCaloClusterizerComponent::Reconfigure(const char* cdbEntry, const char* /*chainId*/)
{  
  // see header file for class documentation

  const char* path="HLT/ConfigPHOS/ClusterizerComponent";

  if (cdbEntry) path = cdbEntry;

  return ConfigureFromCDBTObjString(cdbEntry);
}

int 
AliHLTCaloClusterizerComponent::ScanConfigurationArgument(int argc, const char **argv)
{
  //See header file for documentation

  if(argc <= 0) return 0;

  int i=0;

  TString argument=argv[i];

  if (argument.CompareTo("-digitthreshold") == 0)
    {
      if (++i >= argc) return -EPROTO;
      argument = argv[i];
      fClusterizerPtr->SetEmcMinEnergyThreshold(argument.Atof());
      return 1;
    }

  if (argument.CompareTo("-recpointthreshold") == 0)
    {
      if (++i >= argc) return -EPROTO;
      argument = argv[i];
      fClusterizerPtr->SetEmcClusteringThreshold(argument.Atof());
      return 1;
    }
  return 0;
}

int
AliHLTCaloClusterizerComponent::DoInit(int argc, const char** argv )
{
  //See headerfile for documentation

  
  //

  //  const char *path = "HLT/ConfigPHOS/ClusterizerComponent";

  //  ConfigureFromCDBTObjString(path);

  for (int i = 0; i < argc; i++)
    {
      ScanConfigurationArgument(i, argv);
    }

  return 0;
}

Int_t 
AliHLTCaloClusterizerComponent::CompareDigits(const void *dig0, const void *dig1)
{
  // See header file for documentation
  return (*((AliHLTCaloDigitDataStruct**)(dig0)))->fID - (*((AliHLTCaloDigitDataStruct**)(dig1)))->fID;

  //return (*((AliHLTCaloDigitDataStruct**)(dig0)))->fID - (*((AliHLTCaloDigitDataStruct**)(dig1)))->fID;
}
