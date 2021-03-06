/*
** Copyright (c) 2004 Ingres Corporation
*/
/**
 ** Name: SAXImportHandler.cpp - import XML data into an ingres table format.
 **
 ** Description:
 **
 ** History:
 **      2-Jan-2001 (gupsh01) written.
 **	19-Jul-2001 (hanje04)
 **		Add (char *) cast return of ERx("") as assigning cont char *
 **		to char * causes compile error on Linux
 **	19-Sep-2001 (hanch04)
 **	    Removed the passing of the filename to SAXImportHandler
 **	    file i/o is handled through the xf calls
 **     10-Oct-2001 (hanch04)
 **	    Print out the row data in the same format that copydb -c does so
 **	    that the ascii copy will work for all Ingres data types.
 **	29-Oct-2001 (hanch04)
 **	    If the getValue returns NULL, then copy a string terminator
 **	21-Dec-2001 (hanch04)
 **	    Print out NULL value for columns that are null.
 **	01-Jul-2002 (gupsh01)
 **	    Added support for Binary files for data files that are written 
 **	    for table data by impxml.
 **	11-Apr-2003 (gupsh01)
 **	    Initialize xmlData in the constructor of SAXImportHandlers.cpp. 
 **	    add the code to delete the xmlData list in endDocument funtion.
 **	    Add code to store and handle the correct tag in process_tag. 	
 **	05-June-2003 (gupsh01)
 **	    Modified the process_end_tag function to handle the delimiting
 **	    of data in Xf_xml_data file.
 ** 	07-July-2003 (gupsh01)
 **	    Added dataval and datalen variables, to hold the column data, 
 **	    when processing a columns. A column data is read in several passes 
 **	    when the column contains special characters. 
 **	17-Feb-2004 (hanal04) Bug 111815 INGSRV2709
 **          Prevent memory corruption caused by failure to allocate
 **          memory for NULL terminator in rowformat().
 **	15-Mar-2004 (gupsh01)
 **	    Modified to support Xerces 2.3. 
 **      08-Apr-2004 (gupsh01)
 **          Added isMetaInfoSet, flag to identify if the xml file has the
 **          metadata info. This will ensure that impxml is used only with
 **          conformant xml files, like generated by genxml.
 **	15-Jun-2005 (thaju02)
 **	    rowformat(): for long var[char|byte], break up data into 
 **	    segments. (B114689)
 **	16-Jun-2009 (thich01)
 **	    Treat GEOM type the same as LBYTE.
 **	20-Aug-2009 (thich01)
 **	    Treat all spatial types the same as LBYTE.
**	21-Sep-2009 (hanje04)
**	   Use new include method for stream headers on OSX (DARWIN) to
**	   quiet compiler warnings.
**	   Also add version specific definitions of parser funtions to
**	   allow building against Xerces 2.x and 3.x
**      25-feb-2010 (joea)
**          Add cases for DB_BOO_TYPE in rowformat.
 **/

/*
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999-2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Xerces" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache\@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation, and was
 * originally based on software copyright (c) 1999, International
 * Business Machines, Inc., http://www.ibm.com .  For more information
 * on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

/* ---------------------------------------------------------------------------
**  Includes
** ---------------------------------------------------------------------------
*/

#include <xercesc/util/XMLUni.hpp>
#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/sax/AttributeList.hpp>
#include <xercesc/util/RuntimeException.hpp>
#include "SAXImport.hpp"

/* ---------------------------------------------------------------------------
**  Local const data
**
**  Note: This is the 'safe' way to do these strings. If you compiler supports
**        L"" style strings, and portability is not a concern, you can use
**        those types constants directly.
** ---------------------------------------------------------------------------
*/
static const XMLCh  gEndElement[] = { chOpenAngle, chForwardSlash, chNull };
static const XMLCh  gEndPI[] = { chQuestion, chCloseAngle, chNull };
static const XMLCh  gStartPI[] = { chOpenAngle, chQuestion, chNull };
static const XMLCh  gXMLDecl1[] =
{
        chOpenAngle, chQuestion, chLatin_x, chLatin_m, chLatin_l
    ,   chSpace, chLatin_v, chLatin_e, chLatin_r, chLatin_s, chLatin_i
    ,   chLatin_o, chLatin_n, chEqual, chDoubleQuote, chDigit_1, chPeriod
    ,   chDigit_0, chDoubleQuote, chSpace, chLatin_e, chLatin_n, chLatin_c
    ,   chLatin_o, chLatin_d, chLatin_i, chLatin_n, chLatin_g, chEqual
    ,   chDoubleQuote, chNull
};

static const XMLCh  gXMLDecl2[] =
{
        chDoubleQuote, chQuestion, chCloseAngle
    ,   chCR, chLF, chNull
};

/* ---------------------------------------------------------------------------
**  SAXImportHandlers: Constructors and Destructor
** ---------------------------------------------------------------------------
*/
SAXImportHandlers::SAXImportHandlers( const   char* const encodingName, 
				 const 	XMLFormatter::UnRepFlags unRepFlags, 
				 char 	*currentTagName) :

    fFormatter
    (
        encodingName, 0
        , this
        , XMLFormatter::NoEscapes
        , unRepFlags
    )
{
    /*
    **  Go ahead and output an XML Decl with our known encoding. This
    **  is not the best answer, but its the best we can do until we
    **  have SAX2 support.
    */

    /* initialize the tablist and the indlist and xmlData */
    tablist = NULL;
    indlist = NULL;
    xmlData = NULL;	
    isMetaInfoSet = FALSE;	
    dataval = (char *)MEreqmem(0, DB_MAXSTRING + 32, TRUE, NULL);

    if (currentTagName)
    STcopy(currentTagName, currentTag);

}

SAXImportHandlers::~SAXImportHandlers()
{
    /* cerr << " end of the import Handler " <<endl;  */
}


/* ---------------------------------------------------------------------------
**  SAXImportHandlers: Overrides of the output formatter target interface
** ---------------------------------------------------------------------------
*/
void SAXImportHandlers::writeChars(const XMLByte* const toWrite)
{
    /* fomat each piece mf CDATA value based on other information */
    /* rowformat(currentCol, (char*) toWrite); */
}

 void  SAXImportHandlers::writeChars ( const   XMLByte* const  toWrite
# if XERCES_VERSION_MAJOR > 2
        			     , XMLSize_t    count
# else
        			     , const unsigned int    count
# endif
        			     , XMLFormatter* const   formatter)
{
    /* format each piece of CDATA value based on other information */
    int inlen = STlength((char *)toWrite);

    if (!(isMetaInfoSet))
    {
      cerr<<"ERROR: Either table or table metadata information is not found."<<endl;
      ThrowXML(RuntimeException, XMLExcepts::NoError);
    } 
     
    /* collect the string parsed */
    if (datalen == 0)
    {
      datalen = inlen;
      MEcopy (toWrite, datalen, dataval);
    }	
    else 
    {
      int oldlen = datalen;
      datalen += inlen;
      /* Increase the buffer size to hold the value */
      if (datalen > oldlen)
      {
 	char *temp = dataval;
	int factor = (datalen/(DB_MAXSTRING + 32)) + 1;    
	temp = (char *)MEreqmem(0, (DB_MAXSTRING + 32)*factor, TRUE, NULL);
	MEcopy (dataval, oldlen, temp);
	MEfree(dataval);
	dataval = temp;
      }
      MEcopy (toWrite, inlen, dataval + oldlen);	
    }
}

/* ---------------------------------------------------------------------------
**  SAXImportHandlers: Overrides of the SAX ErrorHandler interface
** ---------------------------------------------------------------------------
*/
void SAXImportHandlers::error(const SAXParseException& e)
{
    cerr << "\nError at file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
         << "\n  Message: " << StrX(e.getMessage()) << endl;
}

void SAXImportHandlers::fatalError(const SAXParseException& e)
{
    cerr << "\nFatal Error at file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
         << "\n  Message: " << StrX(e.getMessage()) << endl;
}

void SAXImportHandlers::warning(const SAXParseException& e)
{
    cerr << "\nWarning at file " << StrX(e.getSystemId())
		 << ", line " << e.getLineNumber()
		 << ", char " << e.getColumnNumber()
         << "\n  Message: " << StrX(e.getMessage()) << endl;
}


/* ---------------------------------------------------------------------------
**  SAXImportHandlers: Overrides of the SAX DTDHandler interface
** ---------------------------------------------------------------------------
*/
void SAXImportHandlers::unparsedEntityDecl(const     XMLCh* const name
                                          , const   XMLCh* const publicId
                                          , const   XMLCh* const systemId
                                          , const   XMLCh* const notationName)
{
    /* Not used at this time */
}


void SAXImportHandlers::notationDecl(const   XMLCh* const name
                                    , const XMLCh* const publicId
                                    , const XMLCh* const systemId)
{
    /* Not used at this time */
}


/* ---------------------------------------------------------------------------
** SAXImportHandlers: Overrides of the SAX DocumentHandler interface
** -------------------------------------------------------------------------
*/
void SAXImportHandlers::characters(const     XMLCh* const    chars
# if XERCES_VERSION_MAJOR > 2
        			     , XMLSize_t    length)
# else
        			     , const unsigned int    length)
# endif
{
    fFormatter.formatBuf(chars, length, XMLFormatter::CharEscapes);
}


void SAXImportHandlers::endDocument()
{
    /* Clean up the lists */
    tab_free(tablist);
    tab_free(indlist);
    deletelist(xmlData);
    if (dataval)
      MEfree(dataval);
}

void SAXImportHandlers::endElement(const XMLCh* const name)
{
	StrX tag = StrX(name);
        char *tag_name = (char *)((tag).localForm());

	/* Save the current tag name */
        if (tag_name)
            STcopy (tag_name, currentTag);

	/* process the end tag */
	process_end_tag(tag_name);
}

void SAXImportHandlers::ignorableWhitespace( const   XMLCh* const chars
# if XERCES_VERSION_MAJOR > 2
        			     , XMLSize_t    length)
# else
        			     , const unsigned int    length)
# endif
{
   /* Ignorable white space occurs outside tags .*/
   /*  fFormatter.formatBuf(chars, length, XMLFormatter::NoEscapes); */
}


void SAXImportHandlers::processingInstruction(const  XMLCh* const target
                                            , const XMLCh* const data)
{
    fFormatter << XMLFormatter::NoEscapes << gStartPI  << target;
	 if (data)
        fFormatter << chSpace << data;
    fFormatter << XMLFormatter::NoEscapes << gEndPI;
	
}

void SAXImportHandlers::startDocument()
{
   /* Do Nothing here */ 
   /* cout<<"Reading the xml document using SAX parser"<<endl; */
}


/* Name: startElement - Start reading the start element in the xml file.
**
** History:
**      10-Dec-2002 (gupsh01)
**         Add delete of allocated memory from the system, after use.
**	02-Jan-2002 (gupsh01)
**	   Removed redeclaration of index variable.
*/
void SAXImportHandlers::startElement(const   XMLCh* const    name,
				       AttributeList&  attributes)
{
    unsigned int index = 0;
    STATUS stat = OK;
    StrX tag = StrX(name);
    char **att_name = new char *[MAXFIELD];
    char **att_value = new char *[MAXFIELD];
    char **att_type = new char *[MAXFIELD];

    char *tag_name = (char *)((tag).localForm());

    if (tag_name)
	STcopy (tag_name, currentTag);

     unsigned int len = attributes.getLength();
     for (index = 0; index < len; index++)
     {
         StrX attname = StrX(attributes.getName(index));
         StrX attvalue = StrX(attributes.getValue(index));
         StrX atttype = StrX(attributes.getType(index));

         char *localname = (char *)((attname).localForm());
         char *localvalue = (char *)((attvalue).localForm());
         char *localtype = (char *)((atttype).localForm());

         att_name[index] = new char[MAXFIELD];
         att_value[index] = new char[MAXFIELD];
         att_type[index] = new char[MAXFIELD];

         STcopy (localname, att_name[index]);
	 if( localvalue != NULL )
	 {
             STcopy (localvalue, att_value[index]);
	 }
	 else
	 {
             STcopy ("\0", att_value[index]);
	 }
         STcopy (localtype, att_type[index]);
     }

    /* Process the start tag */
    stat = process_tag(tag_name, att_name, att_value, len); 

    /* delete the allocated memory */
    for (index = 0; index < len; index++)
    {
	delete att_name[index];
	att_name[index] = 0;

	delete att_value[index];
	att_value[index] = 0;

	delete att_type[index];
	att_type[index] = 0;
    }
    delete att_name;
    delete att_value;
    delete att_type;

    if ( stat )
      ThrowXML(RuntimeException, XMLExcepts::NoError);
}

/* Name process_tag - process the tags read from the xml file
**
** History:
**	06-Dec-2002 (gupsh01)
**	   Removed the code to handle opening the data tables  
**	   from here. We will open them early on when the metadata
**	   information is read.
**	11-Apr-2003 (gupsh01)
**	   move the code for opening the datafile into this function
**	   instead of meta_table_attributes as we need to store it in 
**	   DataHandles array. Also added code for searching for the
**	   handle for the right when handling a table tag.
**      27-Feb-2009 (coomi01) b121663
**         Adapted to flag 64 bit decode format rendering.      
*/
STATUS SAXImportHandlers::
process_tag( char *tag_name, char **att_name, char **value, i2 len)
{
    /* The IngresDoc tag is optional do nothing here */
    if (STequal(tag_name, "IngresDoc"));
    /*         cerr << "started reading the XML document" << endl; */
    else if (STequal (tag_name,"meta_ingres"))
    {
          set_meta_ingres_attributes(att_name,
                                value, len, owner_name);
          /* We assume that the owner name has already been normalized. */
          Owner = (owner_name == NULL ? (char *)ERx("") : owner_name);
    }
    else if (STequal (tag_name,"meta_table"))
    {
        XF_TABINFO  *tab;
	char *outputfilename;

        isMetaInfoSet = TRUE;	
       /* get an empty node */
         tab = get_empty_table();

       set_meta_table_attributes(tab, att_name,
                                value, len);

	/* open a file with this name */
		outputfilename = xfnewfname(tab->name, tab->owner);

		/*
		** The datafiles need to be open in binary mode for
		** windows as, copy -in expects all blob data to be
		** written out in binary mode for windows.
		*/
		DataHandles *temp = new DataHandles();

	# if defined(NT_GENERIC)
		 if ((temp->dataFile =
			 xfopen(outputfilename, TH_BINWRITE)) == NULL)
		 {
			cerr << "Can't open output file " << outputfilename << endl;
			PCexit (FAIL);
		 }
	# else
		 if ((temp->dataFile =
			  xfopen(outputfilename, 0)) == NULL)
		 {
			cerr << "Can't open output file " << outputfilename << endl;
			PCexit (FAIL);
		 }
	# endif /* NT Generic */
			 
		 temp->table_relid = tab->table_reltid;
		 STcopy (outputfilename, temp->datafilename);

		 /* add this handle to xmlData list */
		  
		 if (xmlData == NULL)
		 {
		   xmlData = temp;
		 }
		 else
		 {
		    temp->nextHandle = xmlData->nextHandle;
		    xmlData->nextHandle = temp;
		 }

		/* add this tab to the tabllist */
		tab->tab_next = tablist;
		tablist = tab;
	    }
	    else if (STequal (tag_name,"meta_column"))
	    {
		/*
		** The parent tag for the meta_column will be the
		** meta_table tag.
		*/

		XF_COLINFO  *cp;
		/* get an empty node */

		if (!(tablist) || !(Owner) )
		{
		  cerr<<"ERROR: Column metadata provided without any table information."<<endl;
		  return (FAIL);
		}
		cp = get_empty_column();
		set_tabcol_attributes(cp, att_name,
					  value, len);
		/* add this col to the colllist of the table
		** at the head of the tablist.
		*/
		put_tab_col(tablist,cp);

	    }
	   else if (STequal (tag_name,"meta_index"))
	    {
		XF_TABINFO  *ind;

		if (!(tablist))
		{
		  cerr<<"ERROR: Index metadata provided without any table information."<<endl;
		  return (FAIL);
		}

	       /* get an empty node */
	       ind = get_empty_table();

	       set_meta_index_attributes(ind, att_name,
					value, len);
		STcopy(tablist->name, ind->base_name);
		STcopy(tablist->owner, ind->base_owner);
		/* add this tab to the indlist */
		ind->tab_next = indlist;
		indlist = ind;
	    }
	    else if (STequal (tag_name,"index_column"))
	    {
		XF_COLINFO  *icp;

		if (!indlist)
		{
		  cerr<<"ERROR: Index metadata provided without any table information."<<endl;
		  return (FAIL);
		}
		/* get an empty node */
		icp = get_empty_column();
		set_indcol_attributes(icp, att_name,
					  value, len);
		/* add this col to the indlist of the table
		** at the head of the tablist.
		*/

		put_ind_col(indlist,icp);
	    }
	    else if (STequal (tag_name,"constraints"))
	    {
		/*
		** TO DO: 
		** constraints information
		** We will deal with all kinds of constraints
		** constraints_unique
		**      constraint_unique
		** constraints_check
		**      constraint_check
		** constraints_referential
		**      constraint_referential
		** constraints_foreignKey
		**      constraint_foreignKey
		** constraints_primaryKey
		**      constraint_primaryKey
		*/
	    }
	    else if (STequal (tag_name,"table_location"))
	    {
		if (STequal (att_name[0],"table_path"))
		{

		  if (!(tablist))
		  {
		    cerr<<"ERROR: table_location given but no table information found."<<endl;
		    return (FAIL);
		  }

		    if( STequal (tablist->location, ""))
		     STcopy(value[0], tablist->location);
		    else 
		    {		
		      if (STequal (tablist->location_list, ""))
		      {
			STcopy("Y", tablist->multi_locations);
			STcopy(",", tablist->location_list); 
		      }	
		      else
			STcat(tablist->location_list, ",");
		      STcat(tablist->location_list, value[0]);
		    }
		}
	    }

	    /* Allocate the correct handle to Xf_xmlfile_data */
	    else if (STequal (tag_name,"table"))
	    {
		i2	i=0;
		i4	currentTabNo = 0;

		if (!(isMetaInfoSet))
		{
		  cerr<<"ERROR: Either table or table metadata information is not found."<<endl;
		  return (FAIL);
		}

		for(i=0; i <len; i++) 
		{
		  if (STequal (att_name[i],"table_name"))
		  {
		    STcopy(value[i], currentTab);
		  }
		  if (STequal (att_name[i], "table_nr"))
		  {
		    CVal (value[i], &currentTabNo); 
		  }
		}
			
		/* search for the right handle */
	for (DataHandles *tmphndl = xmlData; tmphndl; 
			tmphndl=tmphndl->nextHandle)
	{
	  if (tmphndl->table_relid == currentTabNo)
	  {
	    Xf_xmlfile_data = tmphndl->dataFile;
	  }
	}
     }     
     else if (STequal (tag_name,"column"))
     {
	i2 i=0;

        if (!(isMetaInfoSet) || !(tablist))
        {
          cerr<<"ERROR: Either table or table metadata information is not found."<<endl;
	  return (FAIL);
        }
	
	/* initialize the datalen for this column */
	datalen = 0; 
	isCurrentColNull = false;
	isCurrentColBase64 = false;
        for(i=0; i <len; i++) 
	{
	    if (STequal (att_name[i],"column_name"))
        	STcopy(value[i], currentCol);
	    else if (STequal (att_name[i],"is_null"))
		isCurrentColNull = true;
	    else if (STequal (att_name[i], "a-dtype") && 
		     STequal (value[i], "bin.base64"))
            {
                isCurrentColBase64 = true;
            }
	}
     }
     else
     {
      /* Either this is the Doctype tag or not a valid tag. 
      ** do not give any error for now 
      **  cerr << "unrecognized tag: " << tag_name;
      **  cerr<< " :unable to interpreted as Ingres Data "<<endl;
      */ 
     }
     return (OK);
}

GLOBALREF bool Portable;

/* {
** Name: process_end_tag - process the ending tags, based on their names.
**
** Description:
**	This is where the metadata is placed in the sql file.
** 
** Inputs:
**     col_name         name of the column to print 
**     data             value to be printed.
**
** Outputs:
**      None
**
** Return:
**      None
**
** History:
**      08-Jun-2001 (gupsh01)
**          Created.
**	14-Jun-2001 (gupsh01)
**	    Added modify statement.
**	11-Dec-2002 (gupsh01)
**	    We should close all the open files. We close the 
**	    datafile when the end of the table tags are 
**	    reached.
**	10-Jun-2003 (gupsh01)
**	    If the length of the data read for column data is 0
**	    we should put 0 in the datafile to avoid error during
**	    copy-in.
**	03-July-2003 (gupsh01)
**	    This routine will now call rowformat to print all the 
**	    data in the data file.
**	07-Dec-2004 (gupsh01)
**	    Fixed datahandler to store the generated name of data
**	    filename.
**	31-Oct-2006 (kiria01) b116944
**	   Increase options passable to xfmodify().
*/
void SAXImportHandlers::
process_end_tag(char *tag_name)
{
    auto DB_DATA_VALUE  dbdv;
    auto DB_USER_TYPE   att_spec;
    char        tbuf[256];
    bool        got_aud_key=FALSE;
    bool        row_label_visible=FALSE;
    bool        firstmodatt = FALSE;
    XF_TABINFO  *tp;
    XF_TABINFO  *ip;
    char        last_owner[FE_MAXNAME + 1];
    char        last_base[FE_MAXNAME + 1];
    char        buf[512];


    *last_owner = *last_base = EOS;

    if (STequal(tag_name,"meta_ingres"))
    {
	bool handlefound = FALSE;
      /*  cerr << "end of meta_ingres flags " <<endl;
        cerr << "dumping the meta data into copy.in file " <<endl;
      */
	
        for (tp = tablist; tp != NULL; tp = tp->tab_next)
        {
	/* correct the sequence of the columns */
	XF_COLINFO  *newlist = mkcollist(tp->column_list);
        col_free(tp->column_list);	
	tp->column_list = newlist;
	
	/* print the table information */
	writecreate(tp, XF_XMLFLAG);

	/* write the modify statements */
	xfmodify(tp, XF_XMLFLAG, 0);

	/* write the copy statements */
	Xf_both = Xf_in;
	Portable = TRUE; 

	/* search for the right handle for this table */
          for (DataHandles *tmphndl = xmlData; tmphndl;
                        tmphndl=tmphndl->nextHandle)
          {
            if (tmphndl->table_relid == tp->table_reltid)
            {
	      writecopy(tp, XF_XMLFLAG, tmphndl->datafilename);
	      handlefound = TRUE;
	      break;
            }
          }

	  if (!handlefound)
	  {
            cerr<<"ERROR: Cannot locate datafilename for "<<tp->name<<endl;
            ThrowXML(RuntimeException, XMLExcepts::NoError);
	  }
	}

	/* print the index information */
	for (ip = indlist; ip != NULL; ip = ip->tab_next)
        {
        /* Does user id have to be reset? */
        xfsetauth(Xf_in, ip->owner);
	/* correct the sequence of the columns */
	XF_COLINFO  *newilist = mkcollist(ip->column_list);
        col_free(ip->column_list);	
	ip->column_list = newilist;

         /* take care of the creation of parallel indexes */
         if (CreateParallelIndexes)
         {
            if (STcmp(last_owner, ip->base_owner) != 0 ||
                     STcmp(last_base, ip->base_name) != 0)
             xfwrite(Xf_in, ERx("create index ("));
             writeindex(ip, XF_XMLFLAG);
             xfwrite(Xf_in, ERx(")"));
             if (ip->tab_next &&
                     STcmp(ip->base_owner, ip->tab_next->base_owner) == 0 &&
                     STcmp(ip->base_name, ip->tab_next->base_name) == 0)
                 xfwrite(Xf_in, ERx(",\n("));
             else
                 xfwrite(Xf_in, GO_STMT);

             STcopy(ip->base_owner, last_owner);
             STcopy(ip->base_name, last_base);
           }
           else
           {
             STprintf(buf, ERx("create %sindex "),
                 (ip->is_unique[0] == 'U' ? ERx("unique ") : ERx("")));
             xfwrite(Xf_in, buf);
             writeindex(ip, XF_XMLFLAG);
             xfwrite(Xf_in, GO_STMT);
           }
        }
    }
    else if (STequal (tag_name,"table"))
    {
      xfclose(Xf_xmlfile_data);
    }
    else if (STequal (tag_name,"column"))
    {
      if (datalen == 0)
      {
	/* format a zero length data string into the text file*/
	char tempdata[] = "";
        rowformat(currentTab, currentCol, isCurrentColNull, isCurrentColBase64, (char*) tempdata);
      }
      else 
      {
	/* Null terminate the dataval string */
        dataval[datalen] = '\0';
        rowformat(currentTab, currentCol, isCurrentColNull, isCurrentColBase64, (char*) dataval);
      }
    }
    else if (STequal (tag_name,"row"))
    {
        if (!(isMetaInfoSet))
        {
          cerr<<"ERROR: Either table or table metadata information is not found."<<endl;
          ThrowXML(RuntimeException, XMLExcepts::NoError);
        }

	xfwrite(Xf_xmlfile_data, ERx("\n"));
    }
}

/* {
** Name: rowformat - formats each output character data in order to
**	             place it in the data file. 
**
** Description:
**	This will obtain the information from the tablist
**	about the column and then format the column 
**	so that it is printed correctly in the form
**	copy operation can upload it into the ingres
**	table.
** Inputs:
**     col_name		name of the column to print 
**     data		value to be printed.
**
** Outputs:
**      None
**
** Return:
**	None
**
** History:
**      08-Jun-2001 (gupsh01)
**          Created.
**	05-Jun-2003 (gupsh01)
**	    Modified this function so it does not write the delimiter, 
**	    here. Also we now handle zero length rows by storing the
**	    datalength in the class level property datalen, removed 
**	    data_len variable from this function.
**	03-July-2003 (gupsh01)
**	    Removed setting the datalen from here. Handle the 
**	    delimited cdata values, &amp, &lt etc.   
**	17-Feb-2004 (hanal04) Bug 111815 INGSRV2709
**          Prevent memory corruption caused by failure to allocate
**          memory for NULL terminator.
**	11-May-2005 (gupsh01)
**	    remove call to norm function to remove leading and 
**	    trailing blanks in the data field. These blanks may be 
**	    present in the date retrieved from database, and should 
**	    not be removed.
**	15-Jun-2005 (thaju02) 
**	    For long var[char|byte], segment data. (B114689)
**	30-Aug-2005 (thaju02)
**	    For blob, null terminate seg[].
**	17-Oct-2005 (thaju02)
**	    Increase field width to account for i8, float8. (B115425)
**	    Object_key/table_key data needs length prefix.
**	03-Aug-2006 (gupsh01)
**	    Added support for new ANSI date/time types.
**      21-Jan-2009 (coomi01) b121370
**          Add support for unicode datatypes NCHAR, NVARCHAR and LONG NVARCHAR.
**      27-Feb-2009 (coomi01) b121663
**         Adapted to decode 64 bit format rendering.      
**
*/
void SAXImportHandlers::
rowformat( char *tab_name, char *col_name, bool is_null, bool is_base64, char *data)
{

    char        *rowbuffer = NULL;
    i2          fillcnt = 0;
    i2          cnt_len = 5;
    i2          buflen;
    i4		norm_len;
    i4          bin_len;
    char 	*instr = data;
    char 	*dataend = NULL;
    char 	*result = NULL;
    char 	*outstr = NULL;
    char	*dataptr = data;
    char	*seg = NULL;

#define SEGSIZE 4096

    /* find the appropriate column information */
    XF_COLINFO *col;
    XF_TABINFO *tp;

    for(tp = tablist; tp; tp = tp->tab_next)
    {
        if (STcompare(tp->name, tab_name) == 0)
	   break;
    }

    if (tp)
    {
        for(col = tp->column_list; col; col = col->col_next)
        {
            if (STcompare(col->column_name, col_name) == 0)
       	    break;
        }    	  
    }
    
    if (!tp || !col)
    {  
        cerr << " error in parsing, no information for ";
        cerr << " table : " << tab_name ;
	cerr << " column : " << col_name << endl;
	return; 
    }

    /* Now format the data into the row buffer 
    ** It may be better if the adf handles this but 
    ** for now we will just do it this way.
    */
    norm_len = STlength(data);

    dataend = data + norm_len;

    if (is_base64)
    {
	bin_len = xf_decode_buffer(data, norm_len, &outstr);
	if (0 == bin_len)
	{
	    /* write the end delimiter */ 
	    if (col->col_next)
		xfwrite(Xf_xmlfile_data, ERx("\t"));
	    else
		xfwrite(Xf_xmlfile_data, ERx("\n"));
	    return;
	}
	STlcopy (outstr, data, bin_len);
    }
    else
    {
        /* We also need to parse the special characters 
        ** &, <, >, ', ", reconvert them to normal characters
        */
        outstr = (char *)MEreqmem (0, norm_len + 1, TRUE, NULL);
        result = outstr;
    
        while (instr < dataend)
        {
	    if (((dataend - instr) >= 5) && 
		(STbcompare(instr, 5, "&amp;", 5, TRUE) == 0))
	    {
		*result++ = '&';
		instr += 5;
		datalen -= 4; 
	    }
	    else if (((dataend - instr) >= 4) && 
		     (STbcompare(instr, 4, "&gt;", 4, TRUE) == 0))
	    {
		*result++ = '>';
		instr += 4;
		datalen -= 3; 
	    }
	    else if (((dataend - instr) >= 4) && 
		     (STbcompare(instr, 4, "&lt;", 4, TRUE) == 0))
	    {
		*result++ = '<';
		instr += 4;
		datalen -= 3; 
	    }
	    else if (((dataend - instr) >= 6) && 
		     (STbcompare(instr, 6, "&apos;", 6, TRUE) == 0))
	    {
		*result++ = '\'';
		instr += 6;
		datalen -= 5; 
	    }
	    else if (((dataend - instr) >= 6) && 
		     (STbcompare(instr, 6, "&quot;", 6, TRUE) == 0))
	    {
		*result++ = '"';
		instr += 6;
		datalen -= 5; 
	    }
	    else
	    {
		*result++ = *instr++;	
	    }
        }
    
        if (result)
          *result = '\0';

	STlcopy (outstr, data, norm_len);
    } 

    if (outstr)
      MEfree (outstr);

    /* length of incoming data has to be consistent with the
    ** definition. Since we have no other valid method to 
    ** deal with this in case the length of data is more
    ** Just truncate the data and give out a warning to the 
    ** Users.
    */
    if (datalen > col->intern_length )
    {
	datalen = col->intern_length;
    }

    rowbuffer = (char *)MEreqmem(0, DB_MAXSTRING + 32, FALSE, NULL);

    if( is_null )
    {
    switch (abs(col->adf_type))
    {
	case DB_NCHR_TYPE:
	case DB_NVCHR_TYPE:
        case DB_CHR_TYPE:
        case DB_CHA_TYPE:
        case DB_TXT_TYPE:
        case DB_VCH_TYPE:
        case DB_VBIT_TYPE:
        case DB_VBYTE_TYPE:
        case DB_BIT_TYPE:
        case DB_BYTE_TYPE:
        case DB_DEC_TYPE:
	    STprintf(rowbuffer, "%5d%s", STlength(data), data);
            break;
        case DB_BOO_TYPE:
            STprintf(rowbuffer, "%8s", data);
            break;
        case DB_INT_TYPE:
        case DB_FLT_TYPE:
        case DB_CPN_TYPE:
        case DB_LOGKEY_TYPE:
        case DB_TABKEY_TYPE:
	    STprintf(rowbuffer, "%-13s", data);
            break;
        case DB_MNY_TYPE:
	    STprintf(rowbuffer, "%-20s", data);
            break;
        case DB_DTE_TYPE:
	    STprintf(rowbuffer, "%-25s", data);
            break;
        case DB_ADTE_TYPE:
	    STprintf(rowbuffer, "%-17s", data);
            break;
        case DB_TMWO_TYPE:
	    STprintf(rowbuffer, "%-21s", data);
            break;
        case DB_TMW_TYPE:
        case DB_TME_TYPE:
	    STprintf(rowbuffer, "%-31s", data);
            break;
        case DB_TSWO_TYPE:
	    STprintf(rowbuffer, "%-39s", data);
            break;
        case DB_TSW_TYPE:
        case DB_TSTMP_TYPE:
	    STprintf(rowbuffer, "%-39s", data);
            break;

	case DB_LNVCHR_TYPE:
        case DB_LVCH_TYPE:
        case DB_LBIT_TYPE:
        case DB_LBYTE_TYPE:
        case DB_GEOM_TYPE:
        case DB_POINT_TYPE:
        case DB_MPOINT_TYPE:
        case DB_LINE_TYPE:
        case DB_MLINE_TYPE:
        case DB_POLY_TYPE:
        case DB_MPOLY_TYPE:
        case DB_GEOMC_TYPE:
	    /* Output as variable length types */
	    /* Embedded nulls will not print for binary
	    ** but this wont work for blobs anyway
	    **/
	    STprintf(rowbuffer, "%-d %s0 ", STlength(data), data);
            break;
	default:
	    STprintf(rowbuffer, "%-s", data);
            break;
    }
    }
    else
    {
    switch (abs(col->adf_type))
    {
	case DB_NCHR_TYPE:
	case DB_NVCHR_TYPE:
        case DB_CHR_TYPE:
        case DB_CHA_TYPE:
        case DB_TXT_TYPE:
        case DB_VCH_TYPE:
        case DB_VBIT_TYPE:
        case DB_VBYTE_TYPE:
        case DB_BIT_TYPE:
        case DB_BYTE_TYPE:
        case DB_DEC_TYPE:
        case DB_LOGKEY_TYPE:
        case DB_TABKEY_TYPE:
	    STprintf(rowbuffer, "%5d%s", STlength(data), data);
            break;
        case DB_BOO_TYPE:
            STprintf(rowbuffer, "%5s", data);
            break;
        case DB_INT_TYPE:
        case DB_FLT_TYPE:
        case DB_CPN_TYPE:
	    STprintf(rowbuffer, "%22s", data);
            break;
        case DB_MNY_TYPE:
	    STprintf(rowbuffer, "%20s", data);
            break;
        case DB_DTE_TYPE:
	    STprintf(rowbuffer, "%-25s", data);
            break;
        case DB_ADTE_TYPE:
	    STprintf(rowbuffer, "%-17s", data);
            break;
        case DB_TMWO_TYPE:
	    STprintf(rowbuffer, "%-21s", data);
            break;
        case DB_TMW_TYPE:
        case DB_TME_TYPE:
	    STprintf(rowbuffer, "%-31s", data);
            break;
        case DB_TSWO_TYPE:
	    STprintf(rowbuffer, "%-39s", data);
            break;
        case DB_TSW_TYPE:
        case DB_TSTMP_TYPE:
	    STprintf(rowbuffer, "%-39s", data);
            break;

	case DB_LNVCHR_TYPE:
        case DB_LVCH_TYPE:
        case DB_LBYTE_TYPE:
        case DB_GEOM_TYPE:
        case DB_POINT_TYPE:
        case DB_MPOINT_TYPE:
        case DB_LINE_TYPE:
        case DB_MLINE_TYPE:
        case DB_POLY_TYPE:
        case DB_MPOLY_TYPE:
        case DB_GEOMC_TYPE:
	    /* Output as variable length types */
	    /* Embedded nulls will not print for binary
	    ** but this wont work for blobs anyway
	    **/
	    datalen = STlength(data);
	    if (datalen > SEGSIZE)
	    {
		seg = (char *)MEreqmem(0, SEGSIZE + 1, FALSE, NULL);
		seg[SEGSIZE] = '\0';
	    }

	    while (datalen > SEGSIZE)
	    {
		MEcopy(dataptr, SEGSIZE, seg);
		STprintf(rowbuffer, "%-d %s", SEGSIZE, seg);
		xfwrite (Xf_xmlfile_data, rowbuffer);
		dataptr += SEGSIZE;
		datalen -= SEGSIZE;
	    }
	    STprintf(rowbuffer, "%-d %s0 ", datalen, dataptr);
	    if (seg)
		MEfree(seg);
	    break;

        case DB_LBIT_TYPE:
	    STprintf(rowbuffer, "%-d %s0 ", STlength(data), data);
            break;

	default:
	    STprintf(rowbuffer, "%s", data);
            break;
    }
    }
    /* print this row into the file buffer */
    xfwrite (Xf_xmlfile_data, rowbuffer);

    /* write the end delimiter */ 
    if (col->col_next)
        xfwrite(Xf_xmlfile_data, ERx("\t"));
    else
        xfwrite(Xf_xmlfile_data, ERx("\n"));

  if (rowbuffer)
    MEfree( rowbuffer );
}

void SAXImportHandlers::
deletelist(DataHandles *list)
{ 
    DataHandles *handler = list->nextHandle;
    DataHandles *temp = NULL; 
		
    while (handler)
    {
      if (handler->dataFile)
        xfclose(handler->dataFile);

      temp = handler;
      handler = handler->nextHandle;
      delete temp;
    }
	   
    if (list->dataFile)
      xfclose(list->dataFile);  
}
