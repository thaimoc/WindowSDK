#include "Transcode.h"

HRESULT CreateMediaSource(const WCHAR *sURL, IMFMediaSource** ppMediaSource);

//-------------------------------------------------------------------
//  CTranscoder constructor
//-------------------------------------------------------------------

CTranscoder::CTranscoder() : 
    m_pSession(NULL),
    m_pSource(NULL),
    m_pTopology(NULL),
    m_pProfile(NULL)
{

}

//-------------------------------------------------------------------
//  CTranscoder destructor
//-------------------------------------------------------------------

CTranscoder::~CTranscoder()
{
    Shutdown();

    SafeRelease(&m_pProfile);
    SafeRelease(&m_pTopology);
    SafeRelease(&m_pSource);
    SafeRelease(&m_pSession);
}


//-------------------------------------------------------------------
//  OpenFile
//        
//  1. Creates a media source for the caller specified URL.
//  2. Creates the media session.
//  3. Creates a transcode profile to hold the stream and 
//     container attributes.
//
//  sURL: Input file URL.
//-------------------------------------------------------------------

HRESULT CTranscoder::OpenFile(const WCHAR *sURL)
{
    if (!sURL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    // Create the media source.
    hr = CreateMediaSource(sURL, &m_pSource);

    //Create the media session.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateMediaSession(NULL, &m_pSession);
    }    

    // Create an empty transcode profile.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateTranscodeProfile(&m_pProfile);
    }
    return hr;
}

struct AACProfileInfo
{
	UINT32  samplesPerSec;
	UINT32  numChannels;
	UINT32  bytesPerSec;
};

AACProfileInfo aac_profiles[] =
{
	{ 44100, 2, 320 },
	{ 44100, 2, 128 },
	{ 44100, 2, 124 },
};

//-------------------------------------------------------------------
//  ConfigureAudioOutput
//        
//  Configures the audio stream attributes.  
//  These values are stored in the transcode profile.
//
//-------------------------------------------------------------------

HRESULT CTranscoder::ConfigureAudioOutput()
{
	assert(m_pProfile);

	HRESULT hr = S_OK;
	DWORD dwMTCount = 0;

	IMFCollection   *pAvailableTypes = NULL;
	IUnknown        *pUnkAudioType = NULL;
	IMFMediaType    *pAudioType = NULL;
	IMFAttributes   *pAudioAttrs = NULL;

	// Get the list of output formats supported by the Windows Media 
	// audio encoder.

	hr = MFTranscodeGetAudioOutputAvailableTypes(
		MFAudioFormat_AAC, // (Win10) only  MFAudioFormat_WMAudioV9/MFAudioFormat_MP3/MFAudioFormat_MPEG/MFAudioFormat_AAC/MFAudioFormat_AMR_NB
		MFT_ENUM_FLAG_ALL,
		NULL,
		&pAvailableTypes
		); 

	// Get the number of elements in the list.

	if (SUCCEEDED(hr))
	{
		hr = pAvailableTypes->GetElementCount(&dwMTCount);

		if (dwMTCount == 0)
		{
			hr = E_UNEXPECTED;
		}
	}

	// In this simple case, use the first media type in the collection.

	if (SUCCEEDED(hr))
	{
		hr = pAvailableTypes->GetElement(0, &pUnkAudioType);
	}

	if (SUCCEEDED(hr))
	{
		hr = pUnkAudioType->QueryInterface(IID_PPV_ARGS(&pAudioType));
	}

	// Create a copy of the attribute store so that we can modify it safely.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateAttributes(&pAudioAttrs, 10);
	}

	if (SUCCEEDED(hr))
	{
		hr = pAudioType->CopyAllItems(pAudioAttrs);
	}

	// Set the encoder to be Windows Media audio encoder, so that the 
	// appropriate MFTs are added to the topology.

	GUID majortype = { 0 };
	GUID subtype = { 0 };

	if (SUCCEEDED(hr))
	{
		hr = pAudioType->GetGUID(MF_MT_MAJOR_TYPE, &majortype);
	}
	if (majortype != MFMediaType_Audio)
	{
		return FAILED(hr);
	}

	if (SUCCEEDED(hr))
	{
		hr = pAudioType->GetGUID(MF_MT_SUBTYPE, &subtype);
	}

	if(subtype != MFAudioFormat_AAC)
	{
		if (SUCCEEDED(hr))
		{
			hr = pAudioAttrs->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		}

		if (SUCCEEDED(hr))
		{
			hr = pAudioAttrs->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, MFGetAttributeUINT32(pAudioType, MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
			hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, MFGetAttributeUINT32(pAudioType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
			hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, MFGetAttributeUINT32(pAudioType, MF_MT_AUDIO_NUM_CHANNELS, 0));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 20000);
			hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, MFGetAttributeUINT32(pAudioType, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
			hr = pAudioAttrs->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, MFGetAttributeUINT32(pAudioType, MF_MT_AAC_PAYLOAD_TYPE, 0));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
			hr = pAudioAttrs->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, MFGetAttributeUINT32(pAudioType, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1);
			hr = pAudioAttrs->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, MFGetAttributeUINT32(pAudioType, MF_MT_AUDIO_BLOCK_ALIGNMENT, 0));
		}
		if (SUCCEEDED(hr))
		{
			//hr = pAudioAttrs->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 0);
			hr = pAudioAttrs->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, MFGetAttributeUINT32(pAudioType, MF_MT_ALL_SAMPLES_INDEPENDENT, 0));
		}
	}

	// Set the attribute store on the transcode profile.
	if (SUCCEEDED(hr))
	{
		hr = m_pProfile->SetAudioAttributes(pAudioAttrs);
	}

	SafeRelease(&pAvailableTypes);
	SafeRelease(&pAudioType);
	SafeRelease(&pUnkAudioType);
	SafeRelease(&pAudioAttrs);

	return hr;
}


//-------------------------------------------------------------------
//  ConfigureContainer
//        
//  Configures the container attributes.  
//  These values are stored in the transcode profile.
//  
//  Note: Setting the container type does not insert the required 
//  MFT node in the transcode topology. The MFT node is based on the 
//  stream settings stored in the transcode profile.
//-------------------------------------------------------------------

HRESULT CTranscoder::ConfigureContainer()
{
    assert (m_pProfile);
    
    HRESULT hr = S_OK;
    
    IMFAttributes* pContainerAttrs = NULL;

    //Set container attributes
    hr = MFCreateAttributes( &pContainerAttrs, 1 );

    //Set the output container to be ASF type
    if (SUCCEEDED(hr))
    {
        hr = pContainerAttrs->SetGUID(
            MF_TRANSCODE_CONTAINERTYPE, 
			MFTranscodeContainerType_ADTS // This output type can be used to convert an AAC stream in the LOAS/LATM format to ADTS format.
            );
    }

    // Use the default setting. Media Foundation will use the stream 
    // settings set in ConfigureAudioOutput and ConfigureVideoOutput.

    if (SUCCEEDED(hr))
    {
        hr = pContainerAttrs->SetUINT32(
            MF_TRANSCODE_ADJUST_PROFILE, 
            MF_TRANSCODE_ADJUST_PROFILE_DEFAULT
            );
    }
	
    //Set the attribute store on the transcode profile.
    if (SUCCEEDED(hr))
    {
        hr = m_pProfile->SetContainerAttributes(pContainerAttrs);
    }

    SafeRelease(&pContainerAttrs);
    return hr;
}

//-------------------------------------------------------------------
//  EncodeToFile
//        
//  Builds the transcode topology based on the input source,
//  configured transcode profile, and the output container settings.  
//-------------------------------------------------------------------
HRESULT CTranscoder::EncodeToFile(const WCHAR *sURL)
{
    assert (m_pSession);
    assert (m_pSource);
    assert (m_pProfile);
    
    if (!sURL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    //Create the transcode topology
    hr = MFCreateTranscodeTopology( m_pSource, sURL, m_pProfile, &m_pTopology );

    // Set the topology on the media session.
    if (SUCCEEDED(hr))
    {
        hr = m_pSession->SetTopology(0, m_pTopology);
    }
    
    //Get media session events. This will start the encoding session.
    if (SUCCEEDED(hr))
    {
        hr = Transcode();
    }

    return hr;
}

//-------------------------------------------------------------------
//  Name: Transcode
//        
//  Start the encoding session by controlling the media session.
//  
//  The encoding starts when the media session raises the 
//  MESessionTopologySet event. The media session is closed after 
//  receiving MESessionEnded. The encoded file is finalized after 
//  the session is closed.
//
//  For simplicity, this sample uses the synchronous method  for 
//  getting media session events. 
//-------------------------------------------------------------------

HRESULT CTranscoder::Transcode()
{
    assert (m_pSession);
    
    IMFMediaEvent* pEvent = NULL;
    MediaEventType meType = MEUnknown;  // Event type

    HRESULT hr = S_OK;
    HRESULT hrStatus = S_OK;            // Event status

    //Get media session events synchronously
    while (meType != MESessionClosed)
    {
        hr = m_pSession->GetEvent(0, &pEvent);

        if (FAILED(hr)) { break; }

        // Get the event type.
        hr = pEvent->GetType(&meType);
        
        if (FAILED(hr)) { break; }

        hr = pEvent->GetStatus(&hrStatus);
        
        if (FAILED(hr)) { break; }

        if (FAILED(hrStatus))
        {
            wprintf_s(L"Failed. 0x%X error condition triggered this event.\n", hrStatus);
            hr = hrStatus;
            break;
        }

        switch (meType)
        {
        case MESessionTopologySet:
            hr = Start();
            if (SUCCEEDED(hr))
            {
                wprintf_s(L"Ready to start.\n");
            }
            break;

        case MESessionStarted:
            wprintf_s(L"Started encoding...\n");
            break;

        case MESessionEnded:
            hr = m_pSession->Close();
            if (SUCCEEDED(hr))
            {
                wprintf_s(L"Finished encoding.\n");
            }
            break;

        case MESessionClosed:
            wprintf_s(L"Output file created.\n");
            break;
        }

        if (FAILED(hr))
        {
            break;
        }

        SafeRelease(&pEvent);
    }

    SafeRelease(&pEvent);
    return hr;
}


//-------------------------------------------------------------------
//  Start
//
//  Starts the encoding session.
//-------------------------------------------------------------------
HRESULT CTranscoder::Start()
{
    assert(m_pSession != NULL);

    HRESULT hr = S_OK;

    PROPVARIANT varStart;
    PropVariantInit(&varStart);

    hr = m_pSession->Start(&GUID_NULL, &varStart);

    if (FAILED(hr))
    {
        wprintf_s(L"Failed to start the session...\n");
    }
    return hr;
}

//-------------------------------------------------------------------
//  Shutdown
//
//  Handler for the MESessionClosed event.
//  Shuts down the media session and the media source.
//-------------------------------------------------------------------

HRESULT CTranscoder::Shutdown()
{
    HRESULT hr = S_OK;

    // Shut down the media source
    if (m_pSource)
    {
        hr = m_pSource->Shutdown();
    }

    // Shut down the media session. (Synchronous operation, no events.)
    if (SUCCEEDED(hr))
    {
        if (m_pSession)
        {
            hr = m_pSession->Shutdown();
        }
    }

    if (FAILED(hr))
    {
        wprintf_s(L"Failed to close the session...\n");
    }
    return hr;
}



///////////////////////////////////////////////////////////////////////
//  CreateMediaSource
//
//  Creates a media source from a URL.
///////////////////////////////////////////////////////////////////////

HRESULT CreateMediaSource(
    const WCHAR *sURL,  // The URL of the file to open.
    IMFMediaSource** ppMediaSource // Receives a pointer to the media source.
    )
{
    if (!sURL)
    {
        return E_INVALIDARG;
    }

    if (!ppMediaSource)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;
    
    MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

    IMFSourceResolver* pSourceResolver = NULL;
    IUnknown* pUnkSource = NULL;

    // Create the source resolver.
    hr = MFCreateSourceResolver(&pSourceResolver);


    // Use the source resolver to create the media source.
    
    if (SUCCEEDED(hr))
    {
        hr = pSourceResolver->CreateObjectFromURL(
            sURL,                       // URL of the source.
            MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
            NULL,                       // Optional property store.
            &ObjectType,                // Receives the created object type. 
            &pUnkSource                 // Receives a pointer to the media source.
            );
    }

    // Get the IMFMediaSource from the IUnknown pointer.
    if (SUCCEEDED(hr))
    {
        hr = pUnkSource->QueryInterface(IID_PPV_ARGS(ppMediaSource));
    }

    SafeRelease(&pSourceResolver);
    SafeRelease(&pUnkSource);
    return hr;
}
