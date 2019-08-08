#include "ResourceConverter.h"
#include "GraphicStructures.h"
#include "3dAnimation.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "png.h"
#include "minizip/zip.h"

static uchar* LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height );
static uchar* LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height );

static Bone* ConvertAssimpPass1( aiScene* ai_scene, aiNode* ai_node );
static void  ConvertAssimpPass2( Bone* root_bone, Bone* parent_bone, Bone* bone, aiScene* ai_scene, aiNode* ai_node );

static void FixTexCoord( float& x, float& y )
{
    if( x < 0.0f )
        x = 1.0f - fmodf( -x, 1.0f );
    else if( x > 1.0f )
        x = fmodf( x, 1.0f );
    if( y < 0.0f )
        y = 1.0f - fmodf( -y, 1.0f );
    else if( y > 1.0f )
        y = fmodf( y, 1.0f );
}

FileManager* ResourceConverter::Convert( const string& name, FileManager& file )
{
    string ext = _str( name ).getFileExtension();
    if( ext == "png" || ext == "tga" )
        return ConvertImage( name, file );
    if( !ext.empty() && Is3dExtensionSupported( ext ) && ext != "fo3d" )
        return Convert3d( name, file );
    file.SwitchToWrite();
    return &file;
}

FileManager* ResourceConverter::ConvertImage( const string& name, FileManager& file )
{
    uchar* data;
    uint   width, height;
    string ext = _str( name ).getFileExtension();
    if( ext == "png" )
        data = LoadPNG( file.GetBuf(), file.GetFsize(), width, height );
    else
        data = LoadTGA( file.GetBuf(), file.GetFsize(), width, height );
    if( !data )
        return nullptr;

    FileManager* converted_file = new FileManager();
    converted_file->SetLEUInt( width );
    converted_file->SetLEUInt( height );
    converted_file->SetData( data, width * height * 4 );

    delete[] data;

    return converted_file;
}

FileManager* ResourceConverter::Convert3d( const string& name, FileManager& file )
{
    // Result bone
    Bone*      root_bone = nullptr;
    AnimSetVec loaded_animations;

    // Extra logging
    if( GameOpt.AssimpLogging )
    {
        aiEnableVerboseLogging( true );
        aiLogStream log_stream = aiGetPredefinedLogStream( aiDefaultLogStream_FILE, "Assimp.log" );
        aiAttachLogStream( &log_stream );
    }

    // Properties
    static aiPropertyStore* import_props;
    if( !import_props )
    {
        import_props = aiCreatePropertyStore();
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, true );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_ALL_MATERIALS, false );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_MATERIALS, true );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_TEXTURES, true );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_CAMERAS, false );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_LIGHTS, false );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_READ_ANIMATIONS, true );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_STRICT_MODE, false );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, true );
        aiSetImportPropertyInteger( import_props, AI_CONFIG_IMPORT_FBX_OPTIMIZE_EMPTY_ANIMATION_CURVES, true );
    }

    // Load scene
    aiScene* scene = (aiScene*) aiImportFileFromMemoryWithProperties( (const char*) file.GetBuf(), file.GetFsize(),
                                                                        aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_GenUVCoords |
                                                                        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                                                                        aiProcess_SortByPType | aiProcess_SplitLargeMeshes | aiProcess_LimitBoneWeights |
                                                                        aiProcess_ImproveCacheLocality, "", import_props );
    if( !scene )
    {
        WriteLog( "Can't load 3d file, name '{}', error '{}'.\n", name, aiGetErrorString() );
        return nullptr;
    }

    // Extract bones
    root_bone = ConvertAssimpPass1( scene, scene->mRootNode );
    ConvertAssimpPass2( root_bone, nullptr, root_bone, scene, scene->mRootNode );

    // Extract animations
    FloatVec      st;
    VectorVec     sv;
    FloatVec      rt;
    QuaternionVec rv;
    FloatVec      tt;
    VectorVec     tv;
    for( unsigned int i = 0; i < scene->mNumAnimations; i++ )
    {
        aiAnimation* anim = scene->mAnimations[ i ];
        AnimSet*     anim_set = new AnimSet();

        for( unsigned int j = 0; j < anim->mNumChannels; j++ )
        {
            aiNodeAnim* na = anim->mChannels[ j ];

            st.resize( na->mNumScalingKeys );
            sv.resize( na->mNumScalingKeys );
            for( unsigned int k = 0; k < na->mNumScalingKeys; k++ )
            {
                st[ k ] = (float) na->mScalingKeys[ k ].mTime;
                sv[ k ] = na->mScalingKeys[ k ].mValue;
            }
            rt.resize( na->mNumRotationKeys );
            rv.resize( na->mNumRotationKeys );
            for( unsigned int k = 0; k < na->mNumRotationKeys; k++ )
            {
                rt[ k ] = (float) na->mRotationKeys[ k ].mTime;
                rv[ k ] = na->mRotationKeys[ k ].mValue;
            }
            tt.resize( na->mNumPositionKeys );
            tv.resize( na->mNumPositionKeys );
            for( unsigned int k = 0; k < na->mNumPositionKeys; k++ )
            {
                tt[ k ] = (float) na->mPositionKeys[ k ].mTime;
                tv[ k ] = na->mPositionKeys[ k ].mValue;
            }

            UIntVec hierarchy;
            aiNode* ai_node = scene->mRootNode->FindNode( na->mNodeName );
            while( ai_node != nullptr )
            {
                hierarchy.insert( hierarchy.begin(), Bone::GetHash( ai_node->mName.data ) );
                ai_node = ai_node->mParent;
            }

            anim_set->AddBoneOutput( hierarchy, st, sv, rt, rv, tt, tv );
        }

        anim_set->SetData( name, anim->mName.data, (float) anim->mDuration, (float) anim->mTicksPerSecond );
        loaded_animations.push_back( anim_set );
    }

    aiReleaseImport( scene );

    // Make new file
    FileManager* converted_file = new FileManager();
    root_bone->Save( *converted_file );
    converted_file->SetBEUInt( (uint) loaded_animations.size() );
    for( size_t i = 0; i < loaded_animations.size(); i++ )
        loaded_animations[ i ]->Save( *converted_file );

    delete root_bone;
    for( size_t i = 0; i < loaded_animations.size(); i++ )
        delete loaded_animations[ i ];

    return converted_file;
}

static Matrix AssimpGlobalTransform( aiNode* ai_node )
{
    return ( ai_node->mParent ? AssimpGlobalTransform( ai_node->mParent ) : Matrix() ) * ai_node->mTransformation;
}

static Bone* ConvertAssimpPass1( aiScene* ai_scene, aiNode* ai_node )
{
    Bone* bone = new Bone();
    bone->NameHash = Bone::GetHash( ai_node->mName.data );
    bone->TransformationMatrix = ai_node->mTransformation;
    bone->GlobalTransformationMatrix = AssimpGlobalTransform( ai_node );
    bone->CombinedTransformationMatrix = Matrix();
    bone->Mesh = nullptr;
    bone->Children.resize( ai_node->mNumChildren );

    for( uint i = 0; i < ai_node->mNumChildren; i++ )
        bone->Children[ i ] = ConvertAssimpPass1( ai_scene, ai_node->mChildren[ i ] );
    return bone;
}

static void ConvertAssimpPass2( Bone* root_bone, Bone* parent_bone, Bone* bone, aiScene* ai_scene, aiNode* ai_node )
{
    for( uint m = 0; m < ai_node->mNumMeshes; m++ )
    {
        aiMesh* ai_mesh = ai_scene->mMeshes[ ai_node->mMeshes[ m ] ];

        // Mesh
        Bone* mesh_bone;
        if( m == 0 )
        {
            mesh_bone = bone;
        }
        else
        {
            mesh_bone = new Bone();
            mesh_bone->NameHash = Bone::GetHash( _str( "{}_{}", ai_node->mName.data, m + 1 ) );
            mesh_bone->CombinedTransformationMatrix = Matrix();
            if( parent_bone )
            {
                parent_bone->Children.push_back( mesh_bone );
                mesh_bone->TransformationMatrix = bone->TransformationMatrix;
                mesh_bone->GlobalTransformationMatrix = AssimpGlobalTransform( ai_node );
            }
            else
            {
                bone->Children.push_back( mesh_bone );
                mesh_bone->TransformationMatrix = Matrix();
                mesh_bone->GlobalTransformationMatrix = AssimpGlobalTransform( ai_node );
            }
        }

        MeshData* mesh = mesh_bone->Mesh = new MeshData();
        mesh->Owner = mesh_bone;

        // Vertices
        mesh->Vertices.resize( ai_mesh->mNumVertices );
        bool has_tangents_and_bitangents = ai_mesh->HasTangentsAndBitangents();
        bool has_tex_coords = ai_mesh->HasTextureCoords( 0 );
        for( uint i = 0; i < ai_mesh->mNumVertices; i++ )
        {
            Vertex3D& v = mesh->Vertices[ i ];
            memzero( &v, sizeof( v ) );
            v.Position = ai_mesh->mVertices[ i ];
            v.Normal = ai_mesh->mNormals[ i ];
            if( has_tangents_and_bitangents )
            {
                v.Tangent = ai_mesh->mTangents[ i ];
                v.Bitangent = ai_mesh->mBitangents[ i ];
            }
            if( has_tex_coords )
            {
                v.TexCoord[ 0 ] = ai_mesh->mTextureCoords[ 0 ][ i ].x;
                v.TexCoord[ 1 ] = 1.0f - ai_mesh->mTextureCoords[ 0 ][ i ].y;
                FixTexCoord( v.TexCoord[ 0 ], v.TexCoord[ 1 ] );
                v.TexCoordBase[ 0 ] = v.TexCoord[ 0 ];
                v.TexCoordBase[ 1 ] = v.TexCoord[ 1 ];
            }
            v.BlendIndices[ 0 ] = -1.0f;
            v.BlendIndices[ 1 ] = -1.0f;
            v.BlendIndices[ 2 ] = -1.0f;
            v.BlendIndices[ 3 ] = -1.0f;
        }

        // Faces
        mesh->Indices.resize( ai_mesh->mNumFaces * 3 );
        for( uint i = 0; i < ai_mesh->mNumFaces; i++ )
        {
            aiFace& face = ai_mesh->mFaces[ i ];
            mesh->Indices[ i * 3 + 0 ] = face.mIndices[ 0 ];
            mesh->Indices[ i * 3 + 1 ] = face.mIndices[ 1 ];
            mesh->Indices[ i * 3 + 2 ] = face.mIndices[ 2 ];
        }

        // Material
        aiMaterial* material = ai_scene->mMaterials[ ai_mesh->mMaterialIndex ];
        aiString    path;
        if( aiGetMaterialTextureCount( material, aiTextureType_DIFFUSE ) )
        {
            aiGetMaterialTexture( material, aiTextureType_DIFFUSE, 0, &path, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
            mesh->DiffuseTexture = path.data;
        }

        // Effect
        mesh->DrawEffect.EffectFilename = nullptr;

        // Skinning
        if( ai_mesh->mNumBones > 0 )
        {
            mesh->SkinBoneNameHashes.resize( ai_mesh->mNumBones );
            mesh->SkinBoneOffsets.resize( ai_mesh->mNumBones );
            mesh->SkinBones.resize( ai_mesh->mNumBones );
            RUNTIME_ASSERT( ai_mesh->mNumBones <= MAX_BONES_PER_MODEL );
            for( uint i = 0; i < ai_mesh->mNumBones; i++ )
            {
                aiBone* ai_bone = ai_mesh->mBones[ i ];

                // Matrices
                Bone* skin_bone = root_bone->Find( Bone::GetHash( ai_bone->mName.data ) );
                if( !skin_bone )
                {
                    WriteLog( "Skin bone '{}' for mesh '{}' not found.\n", ai_bone->mName.data, ai_node->mName.data );
                    skin_bone = bone;
                }
                mesh->SkinBoneNameHashes[ i ] = skin_bone->NameHash;
                mesh->SkinBoneOffsets[ i ] = ai_bone->mOffsetMatrix;
                mesh->SkinBones[ i ] = skin_bone;

                // Blend data
                float bone_index = (float) i;
                for( uint j = 0; j < ai_bone->mNumWeights; j++ )
                {
                    aiVertexWeight& vw = ai_bone->mWeights[ j ];
                    Vertex3D&       v = mesh->Vertices[ vw.mVertexId ];
                    uint            index;
                    if( v.BlendIndices[ 0 ] < 0.0f )
                        index = 0;
                    else if( v.BlendIndices[ 1 ] < 0.0f )
                        index = 1;
                    else if( v.BlendIndices[ 2 ] < 0.0f )
                        index = 2;
                    else
                        index = 3;
                    v.BlendIndices[ index ] = bone_index;
                    v.BlendWeights[ index ] = vw.mWeight;
                }
            }
        }
        else
        {
            mesh->SkinBoneNameHashes.resize( 1 );
            mesh->SkinBoneOffsets.resize( 1 );
            mesh->SkinBones.resize( 1 );
            mesh->SkinBoneNameHashes[ 0 ] = 0;
            mesh->SkinBoneOffsets[ 0 ] = Matrix();
            mesh->SkinBones[ 0 ] = mesh_bone;
            for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
            {
                Vertex3D& v = mesh->Vertices[ i ];
                v.BlendIndices[ 0 ] = 0.0f;
                v.BlendWeights[ 0 ] = 1.0f;
            }
        }

        // Drop not filled indices
        for( size_t i = 0, j = mesh->Vertices.size(); i < j; i++ )
        {
            Vertex3D& v = mesh->Vertices[ i ];
            float     w = 0.0f;
            int       last_bone = 0;
            for( int b = 0; b < BONES_PER_VERTEX; b++ )
            {
                if( v.BlendIndices[ b ] < 0.0f )
                    v.BlendIndices[ b ] = v.BlendWeights[ b ] = 0.0f;
                else
                    last_bone = b;
                w += v.BlendWeights[ b ];
            }
            v.BlendWeights[ last_bone ] += 1.0f - w;
        }
    }

    for( uint i = 0; i < ai_node->mNumChildren; i++ )
        ConvertAssimpPass2( root_bone, bone, bone->Children[ i ], ai_scene, ai_node->mChildren[ i ] );
}

static uchar* LoadPNG( const uchar* data, uint data_size, uint& result_width, uint& result_height )
{
    struct PNGMessage
    {
        static void Error( png_structp png_ptr, png_const_charp error_msg )
        {
            UNUSED_VARIABLE( png_ptr );
            WriteLog( "PNG error '{}'.\n", error_msg );
        }
        static void Warning( png_structp png_ptr, png_const_charp error_msg )
        {
            UNUSED_VARIABLE( png_ptr );
            // WriteLog( "PNG warning '{}'.\n", error_msg );
        }
    };

    // Setup PNG reader
    png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
    if( !png_ptr )
        return nullptr;

    png_set_error_fn( png_ptr, png_get_error_ptr( png_ptr ), &PNGMessage::Error, &PNGMessage::Warning );

    png_infop info_ptr = png_create_info_struct( png_ptr );
    if( !info_ptr )
    {
        png_destroy_read_struct( &png_ptr, nullptr, nullptr );
        return nullptr;
    }

    if( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, nullptr );
        return nullptr;
    }

    static const uchar* data_;
    struct PNGReader
    {
        static void Read( png_structp png_ptr, png_bytep png_data, png_size_t length )
        {
            UNUSED_VARIABLE( png_ptr );
            memcpy( png_data, data_, length );
            data_ += length;
        }
    };
    data_ = data;
    png_set_read_fn( png_ptr, nullptr, &PNGReader::Read );
    png_read_info( png_ptr, info_ptr );

    if( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, nullptr );
        return nullptr;
    }

    // Get information
    png_uint_32 width, height;
    int         bit_depth;
    int         color_type;
    png_get_IHDR( png_ptr, info_ptr, (png_uint_32*) &width, (png_uint_32*) &height, &bit_depth, &color_type, nullptr, nullptr, nullptr );

    // Settings
    png_set_strip_16( png_ptr );
    png_set_packing( png_ptr );
    if( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
        png_set_expand( png_ptr );
    if( color_type == PNG_COLOR_TYPE_PALETTE )
        png_set_expand( png_ptr );
    if( png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS ) )
        png_set_expand( png_ptr );
    png_set_filler( png_ptr, 0x000000ff, PNG_FILLER_AFTER );
    png_read_update_info( png_ptr, info_ptr );

    // Allocate row pointers
    png_bytepp row_pointers = (png_bytepp) malloc( height * sizeof( png_bytep ) );
    if( !row_pointers )
    {
        png_destroy_read_struct( &png_ptr, &info_ptr, nullptr );
        return nullptr;
    }

    // Set the individual row_pointers to point at the correct offsets
    uchar* result = new uchar[ width * height * 4 ];
    for( uint i = 0; i < height; i++ )
        row_pointers[ i ] = result + i * width * 4;

    // Read image
    png_read_image( png_ptr, row_pointers );

    // Clean up
    png_read_end( png_ptr, info_ptr );
    png_destroy_read_struct( &png_ptr, &info_ptr, (png_infopp) nullptr );
    free( row_pointers );

    // Return
    result_width = width;
    result_height = height;
    return result;
}

static uchar* LoadTGA( const uchar* data, uint data_size, uint& result_width, uint& result_height )
{
    // Reading macros
    bool read_error = false;
    uint cur_pos = 0;
    #define READ_TGA( x, len )                          \
        if( !read_error && cur_pos + len <= data_size ) \
        {                                               \
            memcpy( x, data + cur_pos, len );           \
            cur_pos += len;                             \
        }                                               \
        else                                            \
        {                                               \
            memset( x, 0, len );                        \
            read_error = true;                          \
        }

    // Load header
    unsigned char type, pixel_depth;
    short int     width, height;
    unsigned char unused_char;
    short int     unused_short;
    READ_TGA( &unused_char, 1 );
    READ_TGA( &unused_char, 1 );
    READ_TGA( &type, 1 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &unused_char, 1 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &unused_short, 2 );
    READ_TGA( &width, 2 );
    READ_TGA( &height, 2 );
    READ_TGA( &pixel_depth, 1 );
    READ_TGA( &unused_char, 1 );

    // Check for errors when loading the header
    if( read_error )
        return nullptr;

    // Check if the image is color indexed
    if( type == 1 )
        return nullptr;

    // Check for TrueColor
    if( type != 2 && type != 10 )
        return nullptr;

    // Check for RGB(A)
    if( pixel_depth != 24 && pixel_depth != 32 )
        return nullptr;

    // Read
    int    bpp = pixel_depth / 8;
    uint   read_size = height * width * bpp;
    uchar* read_data = new uchar[ read_size ];
    if( type == 2 )
    {
        READ_TGA( read_data, read_size );
    }
    else
    {
        uint  bytes_read = 0, run_len, i, to_read;
        uchar header, color[ 4 ];
        int   c;
        while( bytes_read < read_size )
        {
            READ_TGA( &header, 1 );
            if( header & 0x00000080 )
            {
                header &= ~0x00000080;
                READ_TGA( color, bpp );
                if( read_error )
                {
                    delete[] read_data;
                    return nullptr;
                }
                run_len = ( header + 1 ) * bpp;
                for( i = 0; i < run_len; i += bpp )
                    for( c = 0; c < bpp && bytes_read + i + c < read_size; c++ )
                        read_data[ bytes_read + i + c ] = color[ c ];
                bytes_read += run_len;
            }
            else
            {
                run_len = ( header + 1 ) * bpp;
                if( bytes_read + run_len > read_size )
                    to_read = read_size - bytes_read;
                else
                    to_read = run_len;
                READ_TGA( read_data + bytes_read, to_read );
                if( read_error )
                {
                    delete[] read_data;
                    return nullptr;
                }
                bytes_read += run_len;
                if( bytes_read + run_len > read_size )
                    cur_pos += run_len - to_read;
            }
        }
    }
    if( read_error )
    {
        delete[] read_data;
        return nullptr;
    }

    // Copy data
    uchar* result = new uchar[ width * height * 4 ];
    for( short y = 0; y < height; y++ )
    {
        for( short x = 0; x < width; x++ )
        {
            int i = ( height - y - 1 ) * width + x;
            int j = y * width + x;
            result[ i * 4 + 0 ] = read_data[ j * bpp + 2 ];
            result[ i * 4 + 1 ] = read_data[ j * bpp + 1 ];
            result[ i * 4 + 2 ] = read_data[ j * bpp + 0 ];
            result[ i * 4 + 3 ] = ( bpp == 4 ? read_data[ j * bpp + 3 ] : 0xFF );
        }
    }
    delete[] read_data;

    // Return data
    result_width = width;
    result_height = height;
    return result;
}

bool ResourceConverter::Generate( StrVec* resource_names )
{
    // Generate resources
    bool   something_changed = false;
    StrSet update_file_names;

    for( const string& project_path : ProjectFiles )
    {
        StrVec dummy_vec;
        StrVec check_dirs;
        FileManager::GetFolderFileNames( project_path, true, "", dummy_vec, nullptr, &check_dirs );
        check_dirs.insert( check_dirs.begin(), "" );

        for( const string& check_dir : check_dirs )
        {
            if( !_str( project_path + check_dir ).extractLastDir().compareIgnoreCase( "Resources" ) )
                continue;

            string      resources_root = project_path + check_dir;
            FindDataVec resources_dirs;
            FileManager::GetFolderFileNames( resources_root, false, "", dummy_vec, nullptr, nullptr, &resources_dirs );
            for( const FindData& resource_dir : resources_dirs )
            {
                const string&   res_name = resource_dir.FileName;
                FilesCollection resources( "", resources_root + res_name + "/" );
                if( !resources.GetFilesCount() )
                    continue;

                if( res_name.find( "_Raw" ) == string::npos )
                {
                    string res_name_zip = (string) res_name + ".zip";
                    string zip_path = (string) "Update/" + res_name_zip;
                    bool   skip_making_zip = true;

                    FileManager::CreateDirectoryTree( zip_path );

                    // Check if file available
                    FileManager zip_file;
                    if( !zip_file.LoadFile( zip_path, true ) )
                        skip_making_zip = false;

                    // Test consistency
                    if( skip_making_zip )
                    {
                        zipFile zip = zipOpen( zip_path.c_str(), APPEND_STATUS_ADDINZIP );
                        if( !zip )
                            skip_making_zip = false;
                        else
                            zipClose( zip, nullptr );
                    }

                    // Check timestamps of inner resources
                    while( resources.IsNextFile() )
                    {
                        string       relative_path;
                        FileManager& file = resources.GetNextFile( nullptr, nullptr, &relative_path, true );
                        if( resource_names )
                            resource_names->push_back( relative_path );

                        if( skip_making_zip && file.GetWriteTime() > zip_file.GetWriteTime() )
                            skip_making_zip = false;
                    }

                    // Make zip
                    if( !skip_making_zip )
                    {
                        WriteLog( "Pack resource '{}', files {}...\n", res_name, resources.GetFilesCount() );

                        zipFile zip = zipOpen( ( zip_path + ".tmp" ).c_str(), APPEND_STATUS_CREATE );
                        if( zip )
                        {
                            resources.ResetCounter();
                            while( resources.IsNextFile() )
                            {
                                string       relative_path;
                                FileManager& file = resources.GetNextFile( nullptr, nullptr, &relative_path );
                                FileManager* converted_file = Convert( relative_path, file );
                                if( !converted_file )
                                {
                                    WriteLog( "File '{}' conversation error.\n", relative_path );
                                    continue;
                                }

                                zip_fileinfo zfi;
                                memzero( &zfi, sizeof( zfi ) );
                                if( zipOpenNewFileInZip( zip, relative_path.c_str(), &zfi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_BEST_SPEED ) == ZIP_OK )
                                {
                                    if( zipWriteInFileInZip( zip, converted_file->GetOutBuf(), converted_file->GetOutBufLen() ) )
                                        WriteLog( "Can't write file '{}' in zip file '{}'.\n", relative_path, zip_path );

                                    zipCloseFileInZip( zip );
                                }
                                else
                                {
                                    WriteLog( "Can't open file '{}' in zip file '{}'.\n", relative_path, zip_path );
                                }

                                if( converted_file != &file )
                                    delete converted_file;
                            }
                            zipClose( zip, nullptr );

                            FileManager::DeleteFile( zip_path );
                            if( !FileManager::RenameFile( zip_path + ".tmp", zip_path ) )
                                WriteLog( "Can't rename file '{}' to '{}'.\n", zip_path + ".tmp", zip_path );

                            something_changed = true;
                        }
                        else
                        {
                            WriteLog( "Can't open zip file '{}'.\n", zip_path );
                        }
                    }

                    update_file_names.insert( res_name_zip );
                }
                else
                {
                    bool log_shown = false;
                    while( resources.IsNextFile() )
                    {
                        string       path, relative_path;
                        FileManager& file = resources.GetNextFile( nullptr, &path, &relative_path );
                        string       fname = "Update/" + relative_path;
                        FileManager  update_file;
                        if( !update_file.LoadFile( fname, true ) || file.GetWriteTime() > update_file.GetWriteTime() )
                        {
                            if( !log_shown )
                            {
                                log_shown = true;
                                WriteLog( "Copy resource '{}', files {}...\n", res_name, resources.GetFilesCount() );
                            }

                            FileManager* converted_file = Convert( fname, file );
                            if( !converted_file )
                            {
                                WriteLog( "File '{}' conversation error.\n", fname );
                                continue;
                            }
                            converted_file->SaveFile( fname );
                            if( converted_file != &file )
                                delete converted_file;

                            something_changed = true;
                        }

                        if( resource_names )
                        {
                            string ext = _str( fname ).getFileExtension();
                            if( ext == "zip" || ext == "bos" || ext == "dat" )
                            {
                                DataFile* inner = OpenDataFile( path );
                                if( inner )
                                {
                                    StrVec inner_files;
                                    inner->GetFileNames( "", true, "", inner_files );
                                    resource_names->insert( resource_names->end(), inner_files.begin(), inner_files.end() );
                                    delete inner;
                                }
                                else
                                {
                                    WriteLog( "Can't read data file '{}'.\n", path );
                                }
                            }
                        }

                        update_file_names.insert( relative_path );
                    }
                }
            }
        }
    }

    // Delete unnecessary update files
    FilesCollection update_files( "", "Update/" );
    while( update_files.IsNextFile() )
    {
        string path, relative_path;
        update_files.GetNextFile( nullptr, &path, &relative_path, true );
        if( !update_file_names.count( relative_path ) )
        {
            FileManager::DeleteFile( path );
            something_changed = true;
        }
    }

    return something_changed;
}
