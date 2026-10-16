// Issue #33/#35: the language search subtracts one from an empty table size.
// Issue #36: the resource walker reads a chunk header outside its input range.
// Validate these inputs before calling the original lifted functions. Valid
// data still runs the original code; invalid data stops with an explanation.
void validateResourceChunks(uint32_t data,uint32_t bytes){
    if(!memory->readable(data,bytes))
        throw GuestStop("Datos del juego invalidos: bloque de recursos fuera de memoria; revisa tu copia de NFS Underground 1");
    uint64_t cursor=data,end=uint64_t(data)+bytes;
    while(cursor<end){
        if(end-cursor<8)
            throw GuestStop("Datos del juego truncados: cabecera de recurso incompleta en "+hex(uint32_t(cursor)));
        uint32_t size=read32(uint32_t(cursor)+4);
        if(uint64_t(size)>end-cursor-8)
            throw GuestStop("Datos del juego truncados: recurso "+hex(read32(uint32_t(cursor)))+" excede su bloque; revisa los archivos de tu copia de PC");
        cursor+=8ull+size;
    }
}
void validateLanguageTable(){
    uint32_t count=read32(0x73442c),table=read32(0x734434);
    if(!count||!table||!memory->readable(table,uint64_t(count)*8))
        throw GuestStop("Tabla de idioma vacia o invalida: revisa LANGUAGES y selecciona un idioma disponible de tu copia de NFS Underground 1");
}
void testGuestAssetGuards(){
    State original;original.save();uint32_t data=allocate(64);
    uint32_t oldCount=read32(0x73442c),oldTable=read32(0x734434);
    auto require=[](bool ok){if(!ok)throw GuestStop("Asset guard regression failed");};
    try{
        write32(data,1);write32(data+4,8);write32(data+16,2);write32(data+20,0);
        validateResourceChunks(data,24);validateResourceChunks(0,0);
        for(auto bad:std::array<std::pair<uint32_t,uint32_t>,3>{{{data,7},{data,23},{0,8}}}){
            bool stopped=false;try{validateResourceChunks(bad.first,bad.second);}catch(const GuestStop&){stopped=true;}
            require(stopped);
        }
        write32(data+4,UINT32_MAX);bool stopped=false;
        try{validateResourceChunks(data,24);}catch(const GuestStop&){stopped=true;}require(stopped);
        write32(0x73442c,0);write32(0x734434,0);stopped=false;
        try{callGuest(0x0059fb80u,{});}catch(const GuestStop&){stopped=true;}require(stopped);original.load();
        // Exercise the original search through the manual dispatch and guest
        // calling convention, for present and absent keys in a valid table.
        write32(data,10);write32(data+4,data+32);write32(data+8,20);write32(data+12,data+40);
        write32(0x73442c,2);write32(0x734434,data);
        for(auto item:std::array<std::pair<uint32_t,uint32_t>,3>{{{10,data+32},{20,data+40},{15,0}}}){
            g_edx=item.first;require(callGuest(0x0059fb80u,{})==item.second);
        }
        write32(0x73442c,oldCount);write32(0x734434,oldTable);
        heaps->free(GuestHeaps::process,data);original.load();
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU1","Asset guards passed: chunk bounds, truncated headers, overflow, empty language table and original lookup ABI");
    }catch(...){
        write32(0x73442c,oldCount);write32(0x734434,oldTable);
        heaps->free(GuestHeaps::process,data);original.load();throw;
    }
}
