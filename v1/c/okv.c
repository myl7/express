#include "okv.h"
#include <openssl/rand.h>
#include <omp.h>

#include <math.h>


vatRow db[MAX_DB_SIZE];
int dbSize;
int layers;
uint128_t rerandCounter;
uint128_t *vector;
uint128_t *outVector;
uint8_t *pendingQuery;
uint128_t verificationSeed;
uint128_t rerandSeed;
int pqDataSize;
EVP_CIPHER_CTX *ctx;
EVP_CIPHER_CTX *rerandCtx;
EVP_CIPHER_CTX *newRowCtx;
uint128_t auditCounter;
EVP_CIPHER_CTX *auditCtx;

uint8_t *tempRowId;

int initializeServer(){
    
    vector = (uint128_t*) malloc(MAX_DB_SIZE*16);
    outVector = (uint128_t*) malloc(2*MAX_LAYERS*16);
    
    //set fixed key
    if(!(ctx = EVP_CIPHER_CTX_new())) 
        printf("errors occured in creating context\n");
    unsigned char *aeskey = (unsigned char*) "0123456789123456";
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, aeskey, NULL))
        printf("errors occured in init\n");
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    
    if(!(rerandCtx = EVP_CIPHER_CTX_new())) 
        printf("errors occured in creating context\n");
    //uint128_t aeskey2 = getRandomBlock();
    //next line just for testing. Servers would generate and share a secret key here in production
    unsigned char *aeskey2 = (unsigned char*) "2123456789123456";
    if(1 != EVP_EncryptInit_ex(rerandCtx, EVP_aes_128_ecb(), NULL, aeskey2, NULL))
        printf("errors occured in init\n");
    EVP_CIPHER_CTX_set_padding(rerandCtx, 0);
    
    if(!(newRowCtx = EVP_CIPHER_CTX_new())) 
        printf("errors occured in creating context\n");
    //uint128_t aeskey2 = getRandomBlock();
    //next line just for testing. Servers would generate and share a secret key here in production
    unsigned char *aeskey3 = (unsigned char*) "3123456789123456";
    if(1 != EVP_EncryptInit_ex(newRowCtx, EVP_aes_128_ecb(), NULL, aeskey3, NULL))
        printf("errors occured in init\n");
    EVP_CIPHER_CTX_set_padding(newRowCtx, 0);
    
    if(!(auditCtx = EVP_CIPHER_CTX_new())) 
        printf("errors occured in creating context\n");
    //next line just for testing. Servers would generate and share a secret key here in production
    unsigned char *aeskey4 = (unsigned char*) "4123456789123456";
    if(1 != EVP_EncryptInit_ex(auditCtx, EVP_aes_128_ecb(), NULL, aeskey4, NULL))
        printf("errors occured in init\n");
    EVP_CIPHER_CTX_set_padding(auditCtx, 0);
    
    memset(&rerandSeed, 0, 16);
    
    tempRowId = (uint8_t*) malloc(16);
    
    dbSize = 0;
    layers = 1;
    rerandCounter = 0;
    auditCounter = 0;
    return 0;
}

//register a new entry on a server
int processnewEntry(int dataSize, uint8_t *rowKey){
    
    int len;
    //generate a new rowId
    uint128_t bigCounter = (uint128_t)dbSize;
    if(1 != EVP_EncryptUpdate(newRowCtx, tempRowId, &len, (uint8_t*)&bigCounter, 16))
        printf("errors occured in generating row ID\n");

    uint128_t realRowId;
    memcpy(&realRowId, tempRowId, 16);
    uint128_t realRowKey;
    memcpy(&realRowKey, rowKey, 16);
    
    //print_block(realRowId);
    //printf("\n");
    
    //check if rowId is taken in db and return 1 if that happens
    //we would need a second counter to handle this in reality
    //this could be made more efficient, but I don't really care about optimizing registration time atm
    for(int i = 0; i < dbSize; i++){
        if(memcmp(&realRowId, &(db[i].rowID), 16) == 0){
            printf("row id already taken!\n");
            return 1;
        }
    }
    
    vatRow entry;
    
    if(!(entry.rowKey = EVP_CIPHER_CTX_new())) 
        printf("errors occured in creating context\n");
    if(1 != EVP_EncryptInit_ex(entry.rowKey, EVP_aes_128_ecb(), NULL, (uint8_t*)&realRowKey, NULL))
        printf("errors occured in init\n");
    EVP_CIPHER_CTX_set_padding(entry.rowKey, 0);
    
    entry.rowID = realRowId;
    entry.dataSize = dataSize;
    //entry.rowKey = realRowKey;
    entry.data = malloc(dataSize);
    entry.mask = malloc(dataSize);
    memset(entry.mask, 0 , dataSize);
    memset(entry.data, 0 , dataSize);
    db[dbSize] = entry;
    int i = dbSize;//to make code below work without changing stuff
    dbSize = dbSize + 1;
    layers = ceil(log2(dbSize));
    
    
    //now do the encryption/rerandomization for this entry so it can be retrieved normally
    uint8_t* maskTemp = (uint8_t*) malloc(dataSize+16);
    uint8_t* seedTemp = (uint8_t*) malloc(dataSize+16);
    //get rerandomization mask
    for(int j = 0; j < (db[i].dataSize+16)/16; j++){
        memcpy(&seedTemp[16*j], &rerandSeed, 16);
        seedTemp[16*j] = seedTemp[16*j] ^ j;
    }
    if(1 != EVP_EncryptUpdate(db[i].rowKey, maskTemp, &len, seedTemp, ((dataSize-1)|15)+1))
        printf("errors occured in rerandomization of entry %d\n", i);

    //xor data into db and rerandomize db entry
    for(int j = 0; j < dataSize; j++){
        db[i].data[j] = db[i].data[j] ^ maskTemp[j];
        db[i].mask[j] = maskTemp[j];
    }
    free(maskTemp);
    free(seedTemp);
    return dbSize-1;
}


uint128_t registerQuery(unsigned char* dpfKey, int dataSize, int dataTransferSize){
    //change this to copy the data over instead of setting the pointer so that we're not holding a go pointer after the function returns
    //pendingQuery = dpfKey;
    int len;
    pendingQuery = (uint8_t*) malloc(dataTransferSize);
    memcpy(pendingQuery, dpfKey, dataTransferSize);
    pqDataSize = dataSize;
    //verificationSeed = getRandomBlock();//maybe replace this with something generated from a secret shared between the servers
    if(1 != EVP_EncryptUpdate(auditCtx, (uint8_t*)&verificationSeed, &len, (uint8_t*)&auditCounter, 16))
        printf("errors occured in generating seed\n");
    auditCounter++;
    
    return verificationSeed;
}

//processes query on the server
void processQuery(void){
    
    int len2;
    
    //get rerandomization seed
    if(1 != EVP_EncryptUpdate(rerandCtx, (uint8_t*)&rerandSeed, &len2, (uint8_t*)&rerandCounter, 16))
        printf("errors occured in getting rerandomization seed\n");
    
    uint8_t* dataShare = (uint8_t*) malloc(MAX_DATA_SIZE+16);
    memset(dataShare, 0, MAX_DATA_SIZE+16);
    uint8_t* maskTemp = (uint8_t*) malloc(MAX_DATA_SIZE+16);
    uint8_t* seedTemp = (uint8_t*) malloc(MAX_DATA_SIZE+16);
    
    
    //use the different threads for different queries, not to speed up each query
    //#pragma omp parallel for
    //Note: if putting back this pragma, mallocs above need to go inside loop
    //or be replaced by a big buffer where each thread uses its own part
    for(int i = 0; i < dbSize; i++){
        
        int len;
            
        int ds = db[i].dataSize;
        if(pqDataSize < ds){
            ds = pqDataSize;
        }
        
        //uint8_t* dataShare = (uint8_t*) malloc(db[i].dataSize+16);
        //memset(dataShare, 0, db[i].dataSize+16);
        //uint8_t* maskTemp = (uint8_t*) malloc(db[i].dataSize+16);
        //uint8_t* seedTemp = (uint8_t*) malloc(db[i].dataSize+16);
        
        //run dpf on each input
        vector[i] = evalDPF(ctx, pendingQuery, db[i].rowID, ds, dataShare);
        //print_block(vector[i]);
        
        //get rerandomization mask
        for(int j = 0; j < (db[i].dataSize+16)/16; j++){
            memcpy(&seedTemp[16*j], &rerandSeed, 16);
            seedTemp[16*j] = seedTemp[16*j] ^ j;
        }
        if(1 != EVP_EncryptUpdate(db[i].rowKey, maskTemp, &len, seedTemp, ((db[i].dataSize-1)|15)+1))
            printf("errors occured in rerandomization of entry %d\n", i);
        
        //xor data into db and rerandomize db entry
        for(int j = 0; j < db[i].dataSize; j++){
            if(j < ds) {
                db[i].data[j] = db[i].data[j] ^ dataShare[j];
            }
            db[i].data[j] = db[i].data[j] ^ db[i].mask[j] ^ maskTemp[j];
            db[i].mask[j] = maskTemp[j];
            //printf("%x ", dataShare[j]);
        }
        //printf("\n");
        
        free(dataShare);
        free(maskTemp);
        free(seedTemp);
    }
    //increment rerandomization counter
    rerandCounter++;
    
    //produce verification check for the data
    serverVerify(ctx, verificationSeed, layers, dbSize, vector, outVector);
    
    free(pendingQuery);
}

int getEntrySize(uint8_t *id, int index){
    uint128_t realId;
    memcpy(&realId, id, 16);
    
    if(db[index].rowID == realId){
        return db[index].dataSize;
    }
    else{
        return -1;
    }
}


//read an entry
int readEntry(uint8_t *id, int index, uint8_t *data, uint8_t *seed){
    uint128_t realId;
    memcpy(&realId, id, 16);
    if(db[index].rowID == realId){
        memcpy(data, db[index].data, db[index].dataSize);
        memcpy(seed, &rerandSeed, 16);
        return db[index].dataSize;
    }
    else{
        return -1;
    }
}

int okv_main(){
    initializeServer(0);
    
    //TODO: some testing of the server functionality
    
    
    return 0;
}
