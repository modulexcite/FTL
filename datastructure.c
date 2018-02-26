/* Pi-hole: A black hole for Internet advertisements
*  (c) 2017 Pi-hole, LLC (https://pi-hole.net)
*  Network-wide ad blocking via your own hardware.
*
*  FTL Engine
*  Query processing routines
*
*  This file is copyright under the latest version of the EUPL.
*  Please see LICENSE file for your rights under this license. */

#include "FTL.h"

// converts upper to lower case, and leaves other characters unchanged
void strtolower(char *str)
{
	int i = 0;
	while(str[i]){ str[i] = tolower(str[i]); i++; }
}

void gettimestamp(int *querytimestamp, int *overTimetimestamp)
{
	// Get current time
	*querytimestamp = (int)time(NULL);

	// Floor timestamp to the beginning of 10 minutes interval
	// and add 5 minutes to center it in the interval
	*overTimetimestamp = *querytimestamp-(*querytimestamp%600)+300;
}

int findOverTimeID(int overTimetimestamp)
{
	int timeidx = -1, i;
	// Check struct size
	memory_check(OVERTIME);
	for(i=0; i < counters.overTime; i++)
	{
		validate_access("overTime", i, true, __LINE__, __FUNCTION__, __FILE__);
		if(overTime[i].timestamp == overTimetimestamp)
			return i;
	}
	// We loop over this to fill potential data holes with zeros
	int nexttimestamp = 0;
	if(counters.overTime != 0)
	{
		validate_access("overTime", counters.overTime-1, false, __LINE__, __FUNCTION__, __FILE__);
		nexttimestamp = overTime[counters.overTime-1].timestamp + 600;
	}
	else
	{
		nexttimestamp = overTimetimestamp;
	}

	while(overTimetimestamp >= nexttimestamp)
	{
		// Check struct size
		memory_check(OVERTIME);
		timeidx = counters.overTime;
		validate_access("overTime", timeidx, false, __LINE__, __FUNCTION__, __FILE__);
		// Set magic byte
		overTime[timeidx].magic = MAGICBYTE;
		overTime[timeidx].timestamp = nexttimestamp;
		overTime[timeidx].total = 0;
		overTime[timeidx].blocked = 0;
		overTime[timeidx].cached = 0;
		// overTime[timeidx].querytypedata is static
		overTime[timeidx].clientnum = 0;
		overTime[timeidx].clientdata = NULL;
		memory.querytypedata += (TYPE_MAX-1)*sizeof(int);
		counters.overTime++;

		// Update time stamp for next loop interation
		if(counters.overTime != 0)
		{
			validate_access("overTime", counters.overTime-1, false, __LINE__, __FUNCTION__, __FILE__);
			nexttimestamp = overTime[counters.overTime-1].timestamp + 600;
		}
	}
	return timeidx;
}

int findForwardID(const char * forward, bool count)
{
	int i, forwardID = -1;
	// Go through already knows forward servers and see if we used one of those
	// Check struct size
	memory_check(FORWARDED);
	for(i=0; i < counters.forwarded; i++)
	{
		validate_access("forwarded", i, true, __LINE__, __FUNCTION__, __FILE__);
		if(strcmp(forwarded[i].ip, forward) == 0)
		{
			forwardID = i;
			if(count) forwarded[forwardID].count++;
			return forwardID;
		}
	}
	// This forward server is not known
	// Store ID
	forwardID = counters.forwarded;
	logg("New forward server: %s (%i/%u)", forward, forwardID, counters.forwarded_MAX);

	validate_access("forwarded", forwardID, false, __LINE__, __FUNCTION__, __FILE__);
	// Set magic byte
	forwarded[forwardID].magic = MAGICBYTE;
	// Initialize its counter
	if(count)
		forwarded[forwardID].count = 1;
	else
		forwarded[forwardID].count = 0;
	// Save forward destination IP address
	forwarded[forwardID].ip = strdup(forward);
	memory.forwardedips += (strlen(forward) + 1) * sizeof(char);
	forwarded[forwardID].failed = 0;
	// Increase counter by one
	counters.forwarded++;

	return forwardID;
}

int findDomainID(const char *domain)
{
	int i;
	for(i=0; i < counters.domains; i++)
	{
		validate_access("domains", i, true, __LINE__, __FUNCTION__, __FILE__);
		// Quick test: Does the domain start with the same character?
		if(domains[i].domain[0] != domain[0])
			continue;

		// If so, compare the full domain using strcmp
		if(strcmp(domains[i].domain, domain) == 0)
		{
			domains[i].count++;
			return i;
		}
	}

	// If we did not return until here, then this domain is not known
	// Store ID
	int domainID = counters.domains;
	// // Debug output
	// if(debug) logg("New domain: %s (%i/%i)", domain, domainID, counters.domains_MAX);
	validate_access("domains", domainID, false, __LINE__, __FUNCTION__, __FILE__);
	// Set magic byte
	domains[domainID].magic = MAGICBYTE;
	// Set its counter to 1
	domains[domainID].count = 1;
	// Set blocked counter to zero
	domains[domainID].blockedcount = 0;
	// Initialize wildcard blocking flag with false
	domains[domainID].wildcard = false;
	// Store domain name - no need to check for NULL here as it doesn't harm
	domains[domainID].domain = strdup(domain);
	memory.domainnames += (strlen(domain) + 1) * sizeof(char);
	// Store DNSSEC result for this domain
	domains[domainID].dnssec = DNSSEC_UNSPECIFIED;
	// Set reply points to uninitialized
	domains[domainID].IPv4 = NULL;
	domains[domainID].IPv6 = NULL;
	// Initialize reply type
	domains[domainID].reply[0] = REPLY_UNKNOWN;
	domains[domainID].reply[1] = REPLY_UNKNOWN;
	// Increase counter by one
	counters.domains++;

	return domainID;
}

int findClientID(const char *client)
{
	int i;
	// Compare content of client against known client IP addresses
	for(i=0; i < counters.clients; i++)
	{
		validate_access("clients", i, true, __LINE__, __FUNCTION__, __FILE__);
		// Quick test: Does the clients IP start with the same character?
		if(clients[i].ip[0] != client[0])
			continue;

		// If so, compare the full IP using strcmp
		if(strcmp(clients[i].ip, client) == 0)
		{
			clients[i].count++;
			return i;
		}
	}
	// If we did not return until here, then this client is definitely new
	// Store ID
	int clientID = counters.clients;

	// Debug output
	// if(debug) logg("New client: %s (%i/%i)", client, clientID, counters.clients_MAX);

	validate_access("clients", clientID, false, __LINE__, __FUNCTION__, __FILE__);
	// Set magic byte
	clients[clientID].magic = MAGICBYTE;
	// Set its counter to 1
	clients[clientID].count = 1;
	// Store client IP - no need to check for NULL here as it doesn't harm
	clients[clientID].ip = strdup(client);
	memory.clientips += (strlen(client) + 1) * sizeof(char);
	// Increase counter by one
	counters.clients++;

	return clientID;
}

bool isValidIPv4(const char *addr)
{
	struct sockaddr_in sa;
	return inet_pton(AF_INET, addr, &(sa.sin_addr)) != 0;
}

bool isValidIPv6(const char *addr)
{
	struct sockaddr_in6 sa;
	return inet_pton(AF_INET6, addr, &(sa.sin6_addr)) != 0;
}

int detectStatus(const char *domain)
{
	// Try to find the domain in the array of wildcard blocked domains
	int i;
	for(i=0; i < counters.wildcarddomains; i++)
	{
		validate_access("wildcarddomains", i, false, __LINE__, __FUNCTION__, __FILE__);
		if(strcasecmp(wildcarddomains[i], domain) == 0)
		{
			// Exact match with wildcard domain
			// if(debug)
			// 	printf("%s / %s (exact wildcard match)\n",wildcarddomains[i], domain);
			return 4;
		}
		// Create copy of domain under investigation
		char * part = strdup(domain);
		if(part == NULL)
		{
			// String duplication / memory allocation failed
			logg("Notice: Memory allocation for part in detectStatus failed, domain: \"%s\"", domain);
			continue;
		}
		char * partbuffer = calloc(strlen(part)+1, sizeof(char));
		if(partbuffer == NULL)
		{
			// Memory allocation failed
			logg("Notice: Memory allocation for partbuffer in detectStatus failed, domain: \"%s\"", domain);
			continue;
		}

		// Strip subdomains one after another and
		// compare to existing wildcard entries
		while(sscanf(part,"%*[^.].%s", partbuffer) > 0)
		{
			// Test for a match
			if(strcasecmp(wildcarddomains[i], partbuffer) == 0)
			{
				// Free allocated memory before return'ing
				free(part);
				free(partbuffer);
				// Return match with wildcard domain
				// if(debug)
				// 	printf("%s / %s (wildcard match)\n",wildcarddomains[i], partbuffer);
				return 4;
			}
			if(strlen(partbuffer) > 0)
			{
				// Empty part
				*part = '\0';
				// Replace with partbuffer
				strcat(part, partbuffer);
			}
		}
		// Free allocated memory
		free(part);
		free(partbuffer);
	}

	// If not found -> this answer is not from
	// wildcard blocking, but from e.g. an
	// address=// configuration
	// Answer as "cached"
	return 3;
}
