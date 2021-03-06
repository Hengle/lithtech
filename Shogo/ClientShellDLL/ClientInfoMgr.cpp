#include "ClientInfoMgr.h"
#include "clientheaders.h"
#include "TextHelper.h"
#include "font12.h"
#include "font28.h"
#include "RiotClientShell.h"
#include <stdio.h>

#define VERT_SPACING 3

extern CRiotClientShell* g_pRiotClientShell;

CClientInfoMgr::CClientInfoMgr()
{
	m_pClients = LTNULL;
	m_pClientDE = LTNULL;

	m_nLastFontSize = 0;
	m_bFragSurfacesUpToDate = LTFALSE;

	m_hFragDisplay = LTNULL;
	m_cxFragDisplay = 0;
	m_cyFragDisplay = 0;
}

CClientInfoMgr::~CClientInfoMgr()
{
	if (!m_pClientDE) return;

	ClearAllFragSurfaces();

	CLIENT_INFO* ptr = LTNULL;
	while (m_pClients)
	{
		ptr = m_pClients->pNext;
		m_pClientDE->FreeString (m_pClients->hstrName);
		delete m_pClients;
		m_pClients = ptr;
	}

	if (m_hFragDisplay) m_pClientDE->DeleteSurface (m_hFragDisplay);
}

void CClientInfoMgr::Init (ILTClient* pClientDE)
{
	m_pClientDE = pClientDE;

	UpdateFragDisplay (0);
}

void CClientInfoMgr::AddClient (HSTRING hstrName, uint32 nID, int nFragCount, uint8 r, uint8 g, uint8 b)
{
	if (!m_pClientDE) return;

	// if we already have this client in the list, just return

	CLIENT_INFO* pDup = m_pClients;
	while (pDup)
	{
		if (pDup->nID == nID) return;
		pDup = pDup->pNext;
	}

	// create the new object

	CLIENT_INFO* pNew = new CLIENT_INFO;
	if (!pNew) return;

	pNew->nID = nID;
	pNew->hstrName = hstrName;
	pNew->nFrags = nFragCount;
	pNew->r = r;
	pNew->g = g;
	pNew->b = b;

	// if this client is us, update our frag display

	uint32 nLocalID = 0;
	m_pClientDE->GetLocalClientID (&nLocalID);
	if (pNew->nID == nLocalID)
	{
		UpdateFragDisplay (0);
	}

	// if we don't have a list yet, set the list pointer to the new object

	if (!m_pClients)
	{
		m_pClients = pNew;
		return;
	}

	// we do have a list - add the new object at the end

	CLIENT_INFO* ptr = m_pClients;
	while (ptr->pNext)
	{
		ptr = ptr->pNext;
	}
	ptr->pNext = pNew;
	pNew->pPrev = ptr;

	// if we're drawing all frag surfaces, redo them since this guy may affect the font size

	if (m_bFragSurfacesUpToDate)
	{
		UpdateAllFragSurfaces();
	}
}

void CClientInfoMgr::RemoveClient (uint32 nID)
{
	if (!m_pClientDE || !m_pClients) return;

	// find the client

	CLIENT_INFO* ptr = m_pClients;
	while (ptr)
	{
		if (ptr->nID == nID) break;
		ptr = ptr->pNext;
	}
	if (!ptr) return;

	// remove the client from the list

	if (ptr->pNext) ptr->pNext->pPrev = ptr->pPrev;
	if (ptr->pPrev) ptr->pPrev->pNext = ptr->pNext;
	if (m_pClients == ptr) m_pClients = ptr->pNext;

	m_pClientDE->FreeString (ptr->hstrName);
	delete ptr;

	// if we're drawing all frag surfaces, redo them since this guy may affect the font size

	if (m_bFragSurfacesUpToDate)
	{
		UpdateAllFragSurfaces();
	}
}

void CClientInfoMgr::RemoveAllClients()
{
	if (!m_pClientDE) return;

	ClearAllFragSurfaces();

	CLIENT_INFO* ptr = LTNULL;
	while (m_pClients)
	{
		ptr = m_pClients->pNext;
		m_pClientDE->FreeString (m_pClients->hstrName);
		delete m_pClients;
		m_pClients = ptr;
	}
	
	UpdateFragDisplay (0);
}

void CClientInfoMgr::AddFrag (uint32 nLocalID, uint32 nID)
{
	if (!m_pClientDE || !m_pClients) return;

	// find the client

	CLIENT_INFO* ptr = m_pClients;
	while (ptr)
	{
		if (ptr->nID == nID)
		{
			// add to the frag count
			ptr->nFrags++;
			
			// update the frag display
			if (nLocalID == nID) UpdateFragDisplay (ptr->nFrags);

			// if we're drawing all frag counts, update this client
			if (m_bFragSurfacesUpToDate)
			{
				UpdateSingleFragSurface (ptr);
			}
			
			break;
		}
		ptr = ptr->pNext;
	}
	if (!ptr) return;

	// put this client in the correct position in the list (most frags to least frags)

	CLIENT_INFO* pCurrent = ptr;
	while (ptr->pPrev && pCurrent->nFrags > ptr->pPrev->nFrags)	ptr = ptr->pPrev;
	if (ptr == pCurrent) return;

	// we found a new position - remove current from the list

	pCurrent->pPrev->pNext = pCurrent->pNext;
	if (pCurrent->pNext) pCurrent->pNext->pPrev = pCurrent->pPrev;

	// put us back in in the correct position

	if (!ptr->pPrev)
	{
		m_pClients = pCurrent;
	}

	pCurrent->pPrev = ptr->pPrev;
	pCurrent->pNext = ptr;
	if (ptr->pPrev) ptr->pPrev->pNext = pCurrent;
	ptr->pPrev = pCurrent;
}

void CClientInfoMgr::RemoveFrag (uint32 nLocalID, uint32 nID)
{
	if (!m_pClientDE || !m_pClients) return;

	// find the client

	CLIENT_INFO* ptr = m_pClients;
	while (ptr)
	{
		if (ptr->nID == nID)
		{
			// remove from the frag count
			ptr->nFrags--;
			
			// update the frag display
			if (nLocalID == nID) UpdateFragDisplay (ptr->nFrags);
			
			// if we're drawing all frag counts, update this client
			if (m_bFragSurfacesUpToDate)
			{
				UpdateSingleFragSurface (ptr);
			}
			
			break;
		}
		ptr = ptr->pNext;
	}
	if (!ptr) return;

	// put this client in the correct position in the list (most frags to least frags)

	CLIENT_INFO* pCurrent = ptr;
	while (ptr->pNext && pCurrent->nFrags < ptr->pNext->nFrags)	ptr = ptr->pNext;
	if (ptr == pCurrent) return;

	// we found a new position - remove current from the list

	pCurrent->pNext->pPrev = pCurrent->pPrev;
	if (pCurrent->pPrev) pCurrent->pPrev->pNext = pCurrent->pNext;
	if (m_pClients == pCurrent) m_pClients = pCurrent->pNext;

	// put us back in in the correct position

	pCurrent->pPrev = ptr;
	pCurrent->pNext = ptr->pNext;
	if (ptr->pNext) ptr->pNext->pPrev = pCurrent;
	ptr->pNext = pCurrent;
}

uint32 CClientInfoMgr::GetNumClients()
{
	if (!m_pClientDE) return 0;

	CLIENT_INFO* ptr = m_pClients;

	uint32 nCount = 0;
	while (ptr)
	{
		nCount++;
		ptr = ptr->pNext;
	}

	return nCount;
}

char* CClientInfoMgr::GetPlayerName (uint32 nID)
{
	if (!m_pClientDE) return LTNULL;

	CLIENT_INFO* ptr = m_pClients;
	while (ptr)
	{
		if (ptr->nID == nID) return const_cast<char *>(m_pClientDE->GetStringData (ptr->hstrName));
		ptr = ptr->pNext;
	}

	return "";
}

void CClientInfoMgr::UpdateFragDisplay (int nFrags)
{
	if (!m_pClientDE) return;

	if (m_hFragDisplay) m_pClientDE->DeleteSurface (m_hFragDisplay);

	char str[16];
	itoa (nFrags, str, 10);
	
	m_hFragDisplay = CTextHelper::CreateSurfaceFromString (m_pClientDE, g_pRiotClientShell->GetMenu()->GetFont28s(), str);
	m_pClientDE->GetSurfaceDims (m_hFragDisplay, &m_cxFragDisplay, &m_cyFragDisplay);

	HLTCOLOR hTransColor = m_pClientDE->SetupColor2(0.0f, 0.0f, 0.0f, LTTRUE);
	m_pClientDE->OptimizeSurface (m_hFragDisplay, hTransColor);
}

void CClientInfoMgr::UpdateAllFragSurfaces()
{
	if (!m_pClientDE) return;

	// get our local id
	uint32 nLocalID = 0;
	m_pClientDE->GetLocalClientID (&nLocalID);
	
	// make sure all surfaces are deleted
	ClearAllFragSurfaces();

	// get the screen surface and it's dimensions
	HSURFACE hScreen = m_pClientDE->GetScreenSurface();
	uint32 nWidth = 0;
	uint32 nHeight = 0;
	m_pClientDE->GetSurfaceDims (hScreen, &nWidth, &nHeight);

	// setup the necessary stuff
	HLTCOLOR hTransColor = m_pClientDE->SetupColor1(0.0f, 0.0f, 0.0f, LTTRUE);
	CFont12* pFontNormal = g_pRiotClientShell->GetMenu()->GetFont12n();
	CFont12* pFontSelected = g_pRiotClientShell->GetMenu()->GetFont12s();
	
	// create new surfaces
	CLIENT_INFO* pClient = m_pClients;
	while (pClient)
	{
		char* pstrName = const_cast<char *>(m_pClientDE->GetStringData(pClient->hstrName));
		char nameString[256];
		if(pstrName && pstrName[0])
		{
		}
		else
		{
			pstrName = "unnamed";
		}
		
		LTRect fillRect;
		
		sprintf(nameString, "  (%d) %s", (int)(pClient->m_Ping * 1000.0f), pstrName);
		pClient->hName = CTextHelper::CreateSurfaceFromString (m_pClientDE, (pClient->nID == nLocalID) ? pFontSelected : pFontNormal, nameString);
		
		m_pClientDE->GetSurfaceDims (pClient->hName, &pClient->szName.cx, &pClient->szName.cy);

		fillRect.left = fillRect.top = 0;
		fillRect.right = 10;
		fillRect.bottom = pClient->szName.cy;
		m_pClientDE->FillRect(pClient->hName, &fillRect, SETRGB(220, 220, 220));
		
		fillRect.left++;
		fillRect.top++;
		fillRect.right--;
		fillRect.bottom--;
		m_pClientDE->FillRect(pClient->hName, &fillRect, SETRGB(pClient->r, pClient->g, pClient->b));

		
		m_pClientDE->OptimizeSurface (pClient->hName, hTransColor);

		char strFragCount[16];
		itoa (pClient->nFrags, strFragCount, 10);

		pClient->hFragCount = CTextHelper::CreateSurfaceFromString (m_pClientDE, (pClient->nID == nLocalID) ? pFontSelected : pFontNormal, strFragCount);
		m_pClientDE->GetSurfaceDims (pClient->hFragCount, &pClient->szFragCount.cx, &pClient->szFragCount.cy);
		m_pClientDE->OptimizeSurface (pClient->hFragCount, hTransColor);

		pClient = pClient->pNext;
	}

	// set the flag
	m_bFragSurfacesUpToDate = LTTRUE;
}

void CClientInfoMgr::ClearAllFragSurfaces()
{
	if (!m_pClientDE) return;

	CLIENT_INFO* pClient = m_pClients;
	while (pClient)
	{
		if (pClient->hFragCount) m_pClientDE->DeleteSurface (pClient->hFragCount);
		pClient->hFragCount = LTNULL;
		pClient->szFragCount.cx = pClient->szFragCount.cy = 0;
		
		if (pClient->hName) m_pClientDE->DeleteSurface (pClient->hName);
		pClient->hName = LTNULL;
		pClient->szName.cx = pClient->szName.cy = 0;

		pClient = pClient->pNext;
	}
	
	m_bFragSurfacesUpToDate = LTFALSE;
}

void CClientInfoMgr::UpdateSingleFragSurface (CLIENT_INFO* pClient)
{
	if (!m_pClientDE || !pClient || !m_bFragSurfacesUpToDate) return;

	if (pClient->hFragCount) m_pClientDE->DeleteSurface (pClient->hFragCount);
	pClient->hFragCount = LTNULL;

	// get our local id
	uint32 nLocalID = 0;
	m_pClientDE->GetLocalClientID (&nLocalID);

	// create the font
	CFont12* pFontNormal = g_pRiotClientShell->GetMenu()->GetFont12n();
	CFont12* pFontSelected = g_pRiotClientShell->GetMenu()->GetFont12s();
	
	// create new surfaces
	if (pClient->hFragCount)
	{
		m_pClientDE->DeleteSurface (pClient->hFragCount);
		pClient->hFragCount = LTNULL;
		pClient->szFragCount.cx = pClient->szFragCount.cy = 0;
	}
	
	char strFragCount[12];
	memset (strFragCount, 0, 12);
	itoa (pClient->nFrags, strFragCount, 10);

	pClient->hFragCount = CTextHelper::CreateSurfaceFromString (m_pClientDE, (pClient->nID == nLocalID) ? pFontSelected : pFontNormal, strFragCount);
	m_pClientDE->GetSurfaceDims (pClient->hFragCount, &pClient->szFragCount.cx, &pClient->szFragCount.cy);

	HLTCOLOR hTransColor = m_pClientDE->SetupColor1(0.0f, 0.0f, 0.0f, LTTRUE);
	m_pClientDE->OptimizeSurface (pClient->hFragCount, hTransColor);
}

void CClientInfoMgr::Draw (LTBOOL bDrawSingleFragCount, LTBOOL bDrawAllFragCounts)
{
	if (!m_pClientDE || (!bDrawSingleFragCount && !bDrawAllFragCounts)) return;

	// make sure we're in a network game

	int nGameMode = 0;
	m_pClientDE->GetGameMode(&nGameMode);
	if (nGameMode == STARTGAME_NORMAL || nGameMode == GAMEMODE_NONE) return;

	// see if we need to update all the surfaces

	if (bDrawAllFragCounts && !m_bFragSurfacesUpToDate)
	{
		UpdateAllFragSurfaces();
	}
	else if (!bDrawAllFragCounts && m_bFragSurfacesUpToDate)
	{
		ClearAllFragSurfaces();
	}

	HSURFACE hScreen = m_pClientDE->GetScreenSurface();
	uint32 nScreenWidth = 0;
	uint32 nScreenHeight = 0;
	m_pClientDE->GetSurfaceDims (hScreen, &nScreenWidth, &nScreenHeight);
	int nHalfWidth = (int)nScreenWidth / 2;

	// should we draw our frag count?
	if (bDrawSingleFragCount)
	{
		m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_hFragDisplay, LTNULL, nScreenWidth - m_cxFragDisplay, 0, LTNULL);
	}

	// should we draw all the frag counts?
	
	if (bDrawAllFragCounts && m_bFragSurfacesUpToDate)
	{
		int nTotalHeight = 0;
		int nClients = 0;
		CLIENT_INFO* pClient = m_pClients;
		while (pClient)
		{
			nTotalHeight += __max (pClient->szName.cy, pClient->szFragCount.cy);
			nTotalHeight += VERT_SPACING;
			++nClients;
			pClient = pClient->pNext;
		}
		
		int nY = ((int)nScreenHeight - nTotalHeight) / 2;
		if (nY < 0) nY = 0;

		int i, nSorted;
		CLIENT_INFO *sorted[128], *pClosest;
		if(nClients > 128)
			nClients = 128;

		// Sort them..
		pClosest = LTNULL;
		nSorted = 0;
		pClient = m_pClients;
		while(nSorted < nClients)
		{
			sorted[nSorted++] = pClient;
			pClient = pClient->pNext;
		}

		LTBOOL bHappy = LTFALSE;
		while(!bHappy)
		{
			bHappy = LTTRUE;
			for(i=0; i < (nSorted-1); i++)
			{
				if(sorted[i]->nFrags < sorted[i+1]->nFrags)
				{
					CLIENT_INFO *pTemp = sorted[i];
					sorted[i] = sorted[i+1];
					sorted[i+1] = pTemp;
					bHappy = LTFALSE;
				}
			}
		}


		for(i=0; i < nSorted; i++)
		{
			pClient = sorted[i];

			// Ok.. draw.
			//m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, pClient->hName, LTNULL, nHalfWidth - pClient->szName.cx - 5, nY, LTNULL);
			//m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, pClient->hFragCount, LTNULL, nHalfWidth + 5, nY, LTNULL);

			m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, pClient->hName, LTNULL, 15, nY, LTNULL);
			m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, pClient->hFragCount, LTNULL, pClient->szName.cx + 20, nY, LTNULL);

			nY += __max (pClient->szName.cy, pClient->szFragCount.cy) + VERT_SPACING;

			if (nY + __max (pClient->szName.cy, pClient->szFragCount.cy) > (int)nScreenHeight) break;
		}
	}
}

CLIENT_INFO* CClientInfoMgr::GetClientByID(uint32 nID)
{
	CLIENT_INFO* ptr = m_pClients;
	while (ptr)
	{
		if (ptr->nID == nID) 
			return ptr;

		ptr = ptr->pNext;
	}
	return LTNULL;
}


