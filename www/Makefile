FILES=index.html downloads.html index.css

FILESDIR?=/home/groups/libpkg/htdocs/

all: ${FILES}

install: ${FILES}
	install ${FILES} ${FILESDIR}

clean:
	rm -fr *.html

index.html: index.xml page.xsl nav.xml
	xsltproc index.xml > index.html

downloads.html: downloads.xml page.xsl nav.xml
	xsltproc downloads.xml > downloads.html

page.xsl: nav.xml
