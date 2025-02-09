#pragma once

#include "Common.h"
#include "GraphicStructures.h"
#include "FileUtils.h"

class AnimSet
{
private:
    friend class AnimController;
    struct BoneOutput
    {
        hash          nameHash;
        FloatVec      scaleTime;
        VectorVec     scaleValue;
        FloatVec      rotationTime;
        QuaternionVec rotationValue;
        FloatVec      translationTime;
        VectorVec     translationValue;
    };
    typedef vector< BoneOutput > BoneOutputVec;

    string        animFileName;
    string        animName;
    float         durationTicks;
    float         ticksPerSecond;
    BoneOutputVec boneOutputs;
    HashVecVec    bonesHierarchy;

public:
    void          SetData( const string& fname, const string& name, float ticks, float tps );
    void          AddBoneOutput( HashVec hierarchy, const FloatVec& st, const VectorVec& sv, const FloatVec& rt, const QuaternionVec& rv, const FloatVec& tt, const VectorVec& tv );
    const string& GetFileName();
    const string& GetName();
    uint          GetBoneOutputCount();
    float         GetDuration();
    HashVecVec& GetBonesHierarchy();
    void        Save( File& file );
    void        Load( File& file );
};
typedef vector< AnimSet* > AnimSetVec;

class AnimController
{
private:
    struct Output
    {
        hash          nameHash;
        Matrix*       matrix;
        // Data for tracks blending
        BoolVec       valid;
        FloatVec      factor;
        VectorVec     scale;
        QuaternionVec rotation;
        VectorVec     translation;
    };
    typedef vector< Output >  OutputVec;
    typedef vector< Output* > OutputPtrVec;

    struct Track
    {
        struct Event
        {
            enum EType { Enable, Speed, Weight };
            Event( EType t, float v, float start, float smooth ): type( t ), valueFrom( -1.0f ), valueTo( v ), startTime( start ), smoothTime( smooth ) {}
            EType type;
            float valueFrom;
            float valueTo;
            float startTime;
            float smoothTime;
        };
        typedef vector< Event > EventVec;

        bool         enabled;
        float        speed;
        float        weight;
        float        position;
        AnimSet*     anim;
        OutputPtrVec animOutput;
        EventVec     events;
    };
    typedef vector< Track > TrackVec;

    bool        cloned;
    AnimSetVec* sets;
    OutputVec*  outputs;
    TrackVec    tracks;
    float       curTime;
    bool        interpolationDisabled;

public:
    AnimController();
    ~AnimController();
    static AnimController* Create( uint track_count );
    AnimController*        Clone();
    void                   RegisterAnimationOutput( hash bone_name_hash, Matrix& output_matrix );
    void                   RegisterAnimationSet( AnimSet* animation );
    AnimSet*               GetAnimationSet( uint index );
    AnimSet*               GetAnimationSetByName( const string& name );
    float                  GetTrackPosition( uint track );
    uint                   GetNumAnimationSets();
    void                   SetTrackAnimationSet( uint track, AnimSet* anim );
    void                   ResetBonesTransition( uint skip_track, const HashVec& bone_name_hashes );

    void  Reset();
    float GetTime();
    void  AddEventEnable( uint track, bool enable, float start_time );
    void  AddEventSpeed( uint track, float speed, float start_time, float smooth_time );
    void  AddEventWeight( uint track, float weight, float start_time, float smooth_time );
    void  SetTrackEnable( uint track, bool enable );
    void  SetTrackPosition( uint track, float position );
    void  SetInterpolation( bool enabled );
    void  AdvanceTime( float time );

private:
    template< class T >
    void FindSRTValue( float time, FloatVec& times, vector< T >& values, T& result )
    {
        for( uint n = 0, m = (uint) times.size(); n < m; n++ )
        {
            if( n + 1 < m )
            {
                if( time >= times[ n ] && time < times[ n + 1 ] )
                {
                    result = values[ n ];
                    T&    value = values[ n + 1 ];
                    float factor = ( time - times[ n ] ) / ( times[ n + 1 ] - times[ n ] );
                    Interpolate( result, value, factor );
                    return;
                }
            }
            else
            {
                result = values[ n ];
            }
        }
    }

    void Interpolate( Quaternion& q1, const Quaternion& q2, float factor );
    void Interpolate( Vector& v1, const Vector& v2, float factor );
};
