<?xml version='1.0'?>
<!--<!DOCTYPE xsl:stylesheet
[
  <!ENTITY % site-entities SYSTEM "entities.site">
  %site-entities;
]>-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

 <xsl:output method="html" />

 <xsl:template match="nav">
  <ul>
   <xsl:for-each select="page">
    <li>
     <a href="{file}.html">
      <xsl:value-of select="title" />
     </a>
    </li>
   </xsl:for-each>
  </ul>
 </xsl:template>

 <xsl:template name="head">
  <head>
   <title><xsl:value-of select="title" /></title>
    <link rel="stylesheet" type="text/css" href="index.css" />
  </head>
 </xsl:template>

 <xsl:template name="body">
  <body>
   <div id="title">
     <h1><xsl:value-of select="title" /></h1>
   </div>
   <div id="nav">
    <xsl:apply-templates select="document('nav.xml')"/>
   </div>
   <div id="main">
    <xsl:for-each select="body">
     <xsl:copy-of select="node()" />
    </xsl:for-each>
   </div>
   <div id="footer">
    <a href="http://developer.berlios.de" title="BerliOS Developer">
     <img src="http://developer.berlios.de/bslogo.php?group_id=5252" width="124px" height="32px" border="0" alt="BerliOS Developer Logo" />
    </a>
   </div>
  </body>
 </xsl:template>

 <xsl:template name="page">
  <xsl:variable name="this_file" select="page[name]" />
  <html>
   <xsl:call-template name="head" />
   <xsl:call-template name="body" />
  </html>
 </xsl:template>


 <xsl:template match="page">
  <xsl:call-template name="page" />
 </xsl:template>
</xsl:stylesheet>
