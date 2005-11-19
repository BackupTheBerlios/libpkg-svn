FILES=index.html downloads.html index.css

FILESDIR?=/home/groups/libpkg/htdocs/test/

all: ${FILES}

install: ${FILES}
	install ${FILES} ${FILESDIR}

clean:
	rm -fr *.html

index.html: index.xml page.xsl
	xsltproc index.xml > index.html

downloads.html: downloads.xml page.xsl
	xsltproc downloads.xml > downloads.html

index.xsl: nav.xml
