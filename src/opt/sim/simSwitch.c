/**CFile****************************************************************

  FileName    [simSwitch.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Computes switching activity of nodes in the ABC network.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: simSwitch.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "sim.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static void Sim_NodeSimulate( Abc_Obj_t * pNode, Vec_Ptr_t * vSimInfo, int nSimWords );
static float Sim_ComputeSwitching( unsigned * pSimInfo, int nSimWords );
 
////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////


int Sim_NtkWriteSwitchingTemplate( Abc_Ntk_t * pNtk, const char * fileBaseName )
{
    FILE * pFile;
    Abc_Obj_t * pNode;
    int i;

    // Validate the input network and base file name
    if ( pNtk == NULL || fileBaseName == NULL )
    {
        fprintf(stderr, "Error: Invalid network or file base name.\n");
        return -1;
    }

    // Construct the output file name with ".switch" extension
    char * fileName = (char *)malloc(strlen(fileBaseName) + 8); // +8 for ".switch" and null terminator
    if ( fileName == NULL )
    {
        fprintf(stderr, "Error: Memory allocation failed for file name.\n");
        return -1;
    }
    sprintf(fileName, "%s.switch", fileBaseName);

    // Open the file for writing
    pFile = fopen(fileName, "w");
    if ( pFile == NULL )
    {
        fprintf(stderr, "Error: Could not open file %s for writing.\n", fileName);
        free(fileName);
        return -1;
    }

    // Write header information
    fprintf(pFile, "# Switching Activities Template File\n");
    fprintf(pFile, "# Format: ID [Switching Value Placeholder]\n");
    fprintf(pFile, "# CIs: %d\n", Abc_NtkCiNum(pNtk));
    fprintf(pFile, "# Nodes: %d\n\n", Abc_NtkObjNumMax(pNtk));

    // Write CI information
    //fprintf(pFile, "# [CI Switching Activities]\n");
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        fprintf(pFile, "CI %d: ID=%d .5\n", i, pNode->Id - 1);
    }
    //fprintf(pFile, "\n");

    // Write Node information
    //fprintf(pFile, "# [Node Switching Activities]\n");
    Vec_Ptr_t * vNodes = Abc_AigDfs( pNtk, 1, 0 ); // DFS to get internal nodes
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    {
        fprintf(pFile, "Node %d: ID=%d .5\n", i, pNode->Id - 1);
    }

    // Clean up
    Vec_PtrFree(vNodes);
    fclose(pFile);

    fprintf(stdout, "Switching template file written to %s\n", fileName);
    free(fileName);
    return 0;
}
/**Function*************************************************************

  Synopsis    [Loads switching activity from char * FileName.]

  Description [Load switching activity and assign to Inputs and Internal Nodes.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Sim_NtkLoadSwitching( Abc_Ntk_t * pNtk, char * fileName )
{
    Vec_Int_t * vSwitching;
    float * pSwitching;
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pNode;
    float * Ci_Switching;
    float * Node_Switching;
    int i, nCis, nNodes;
    FILE * pFile;

    vNodes = Abc_AigDfs( pNtk, 1, 0 );
    nNodes = Vec_IntStart( Abc_NtkObjNumMax(pNtk) );
    // Validate the input file name
    if ( fileName == NULL )
    {
        fprintf(stderr, "Warning: Invalid switching file name or not provided >> Generating a template now\n");
        Sim_NtkWriteSwitchingTemplate( pNtk, pNtk->pName );
        return NULL;
    }

    // Open the file for reading
    pFile = fopen(fileName, "r");
    if ( pFile == NULL )
    {
        fprintf(stderr, "Error: Could not open file %s for reading.\n", fileName);
        return NULL;
    }

    // Allocate memory for switching arrays
    nCis = Abc_NtkCiNum(pNtk);
    //nNodes = Abc_NtkObjNumMax(pNtk);
    Ci_Switching = (float *)malloc(nCis * sizeof(float));
    Node_Switching = (float *)malloc(nNodes * sizeof(float));
    if ( Ci_Switching == NULL || Node_Switching == NULL )
    {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        fclose(pFile);
        return NULL;
    }

    // Skip header lines
    char buffer[256];
    while ( fgets(buffer, sizeof(buffer), pFile) )
    {
        if ( buffer[0] != '#' && buffer[0] != '\n' )
            break;
    }

    // Read CI switching values
    for ( i = 0; i < nCis; i++ )
    {
        int id;
        float value;

        // Debug: Log the line being read
        printf("DEBUG: Line read for CI: '%s'\n", buffer);

        // Parse the line: "CI <index>: ID=<id> <value>"
        if ( sscanf(buffer, "CI %*d: ID=%d %f", &id, &value) != 2 )
        {
            fprintf(stderr, "Error: Failed to parse CI %d line: '%s'\n", i, buffer);
            free(Ci_Switching);
            free(Node_Switching);
            fclose(pFile);
            return NULL;
        }
        Ci_Switching[id] = value; // Store the switching value
        if ( fgets(buffer, sizeof(buffer), pFile) == NULL && i < nCis - 1 )
        {
            fprintf(stderr, "Error: Unexpected end of file while reading CI values.\n");
            free(Ci_Switching);
            free(Node_Switching);
            fclose(pFile);
            return NULL;
        }
    }


    // Read Node switching values
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    //for ( i = 0; i < nNodes; i++ )
    {
        int id;
        float value;

        // Debug: Log the line being read
        printf("DEBUG: Line read for Node: '%s'\n", buffer);

        // Parse the line: "Node <index>: ID=<id> <value>"
        if ( sscanf(buffer, " Node %*d : ID = %d %f ", &id, &value) != 2 )
        {
            fprintf(stderr, "Error: Failed to parse Node %d line: '%s'\n", i, buffer);
            free(Ci_Switching);
            free(Node_Switching);
            fclose(pFile);
            return NULL;
        }

        Node_Switching[id] = value; // Store the switching value

        // Read the next line for the next node
        if ( fgets(buffer, sizeof(buffer), pFile) == NULL && i < nNodes - 1 )
        {
            fprintf(stderr, "Error: Unexpected end of file while reading Node values.\n");
            free(Ci_Switching);
            free(Node_Switching);
            fclose(pFile);
            return NULL;
        }
    }
    fclose(pFile);

    // Initialize vector for switching values
    vSwitching = Vec_IntStart(nNodes);
    if ( vSwitching == NULL )
    {
        fprintf(stderr, "Error: Memory allocation failed for vSwitching.\n");
        free(Ci_Switching);
        free(Node_Switching);
        return NULL;
    }
    pSwitching = (float *)vSwitching->pArray;
    
    // Assign CI switching values
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        pSwitching[pNode->Id] = Ci_Switching[i];
    }

    // Assign node switching values
    vNodes = Abc_AigDfs( pNtk, 1, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    {
        printf("Loading node:%i = pSwitching:%.5f\n", 
              pNode->Id, Node_Switching[pNode->Id]);
        pSwitching[pNode->Id] = Node_Switching[pNode->Id];
    }
    printf("finished loading\n");
    // Free allocated resources
    Vec_PtrFree(vNodes);
    free(Ci_Switching);
    free(Node_Switching);
    return vSwitching;
}


/**Function*************************************************************

  Synopsis    [Computes switching activity using simulation.]

  Description [Computes switching activity, which is understood as the
  probability of switching under random simulation. Assigns the
  random simulation information at the CI and propagates it through
  the internal nodes of the AIG.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Sim_NtkComputeSwitching( Abc_Ntk_t * pNtk, int nPatterns )
{
    Vec_Int_t * vSwitching;
    float * pSwitching;
    Vec_Ptr_t * vNodes;
    Vec_Ptr_t * vSimInfo;
    Abc_Obj_t * pNode;
    unsigned * pSimInfo;
    int nSimWords, i;

    // allocate space for simulation info of all nodes
    nSimWords = SIM_NUM_WORDS(nPatterns);
    vSimInfo = Sim_UtilInfoAlloc( Abc_NtkObjNumMax(pNtk), nSimWords, 0 );
    // assign the random simulation to the CIs
    vSwitching = Vec_IntStart( Abc_NtkObjNumMax(pNtk) );
    pSwitching = (float *)vSwitching->pArray;
    Abc_NtkForEachCi( pNtk, pNode, i )
    {
        pSimInfo = (unsigned *)Vec_PtrEntry(vSimInfo, pNode->Id);
        Sim_UtilSetRandom( pSimInfo, nSimWords );
        pSwitching[pNode->Id] = Sim_ComputeSwitching( pSimInfo, nSimWords );
    }
    // simulate the internal nodes
    vNodes  = Abc_AigDfs( pNtk, 1, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i )
    {
        pSimInfo = (unsigned *)Vec_PtrEntry(vSimInfo, pNode->Id);
        Sim_UtilSimulateNodeOne( pNode, vSimInfo, nSimWords, 0 );
        //Vec_IntPrint(&vSimInfo);
        //Vec_VecPrintInt(&vSimInfo,0);
        pSwitching[pNode->Id] = Sim_ComputeSwitching( pSimInfo, nSimWords );
        //printf("node:%i; nSim:%i; pSimInfo:%i; pSwitching:%.5f\n", pNode->Id, nSimWords, &pSimInfo,pSwitching[pNode->Id]);
    }
    Vec_PtrFree( vNodes );
    Sim_UtilInfoFree( vSimInfo );
    return vSwitching;
}

/**Function*************************************************************

  Synopsis    [Computes switching activity of one node.]

  Description [Uses the formula: Switching = 2 * nOnes * nZeros / (nTotal ^ 2) ]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
float Sim_ComputeSwitching( unsigned * pSimInfo, int nSimWords )
{
    int nOnes, nTotal;
    nTotal = 32 * nSimWords;
    nOnes = Sim_UtilCountOnes( pSimInfo, nSimWords );
    return (float)2.0 * nOnes / nTotal * (nTotal - nOnes) / nTotal;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

