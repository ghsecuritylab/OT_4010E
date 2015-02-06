

#include "config.h"
#include "HTMLEmbedElement.h"

#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "MappedAttribute.h"
#include "RenderEmbeddedObject.h"
#include "RenderImage.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLEmbedElement::HTMLEmbedElement(const QualifiedName& tagName, Document* document)
    : HTMLPlugInImageElement(tagName, document)
    , m_needWidgetUpdate(false)
{
    ASSERT(hasTagName(embedTag));
}

PassRefPtr<HTMLEmbedElement> HTMLEmbedElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLEmbedElement(tagName, document));
}

static inline RenderWidget* findWidgetRenderer(const Node* n) 
{
    if (!n->renderer())
        do
            n = n->parentNode();
        while (n && !n->hasTagName(objectTag));

    if (n && n->renderer() && n->renderer()->isWidget())
        return toRenderWidget(n->renderer());

    return 0;
}

RenderWidget* HTMLEmbedElement::renderWidgetForJSBindings() const
{
    document()->updateLayoutIgnorePendingStylesheets();
    return findWidgetRenderer(this);
}

bool HTMLEmbedElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == hiddenAttr) {
        result = eUniversal;
        return false;
    }
        
    return HTMLPlugInElement::mapToEntry(attrName, result);
}

void HTMLEmbedElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
  
    if (attr->name() == typeAttr) {
        m_serviceType = value.string().lower();
        int pos = m_serviceType.find(";");
        if (pos != -1)
            m_serviceType = m_serviceType.left(pos);
        if (!isImageType() && m_imageLoader)
            m_imageLoader.clear();
    } else if (attr->name() == codeAttr)
        m_url = deprecatedParseURL(value.string());
    else if (attr->name() == srcAttr) {
        m_url = deprecatedParseURL(value.string());
        if (renderer() && isImageType()) {
            if (!m_imageLoader)
                m_imageLoader.set(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        }
    } else if (attr->name() == hiddenAttr) {
        if (equalIgnoringCase(value.string(), "yes") || equalIgnoringCase(value.string(), "true")) {
            // FIXME: Not dynamic, since we add this but don't remove it, but it may be OK for now
            // that this rarely-used attribute won't work properly if you remove it.
            addCSSLength(attr, CSSPropertyWidth, "0");
            addCSSLength(attr, CSSPropertyHeight, "0");
        }
    } else if (attr->name() == nameAttr) {
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeNamedItem(m_name);
            document->addNamedItem(value);
        }
        m_name = value;
    } else
        HTMLPlugInElement::parseMappedAttribute(attr);
}

bool HTMLEmbedElement::rendererIsNeeded(RenderStyle* style)
{
    if (isImageType())
        return HTMLPlugInElement::rendererIsNeeded(style);

    Frame* frame = document()->frame();
    if (!frame)
        return false;

    Node* p = parentNode();
    if (p && p->hasTagName(objectTag)) {
        ASSERT(p->renderer());
        return false;
    }

#if ENABLE(DASHBOARD_SUPPORT)
    // Workaround for <rdar://problem/6642221>. 
    if (Settings* settings = frame->settings()) {
        if (settings->usesDashboardBackwardCompatibilityMode())
            return true;
    }
#endif

    return HTMLPlugInElement::rendererIsNeeded(style);
}

RenderObject* HTMLEmbedElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (isImageType())
        return new (arena) RenderImage(this);
    return new (arena) RenderEmbeddedObject(this);
}

void HTMLEmbedElement::attach()
{
    m_needWidgetUpdate = true;

    bool isImage = isImageType();

    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer()) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();

        if (renderer())
            toRenderImage(renderer())->setCachedImage(m_imageLoader->image());
    }
}

void HTMLEmbedElement::updateWidget()
{
    document()->updateStyleIfNeeded();
    if (m_needWidgetUpdate && renderer() && !isImageType())
        toRenderEmbeddedObject(renderer())->updateWidget(true);
}

void HTMLEmbedElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->addNamedItem(m_name);

    String width = getAttribute(widthAttr);
    String height = getAttribute(heightAttr);
    if (!width.isEmpty() || !height.isEmpty()) {
        Node* n = parent();
        while (n && !n->hasTagName(objectTag))
            n = n->parent();
        if (n) {
            if (!width.isEmpty())
                static_cast<HTMLObjectElement*>(n)->setAttribute(widthAttr, width);
            if (!height.isEmpty())
                static_cast<HTMLObjectElement*>(n)->setAttribute(heightAttr, height);
        }
    }

    HTMLPlugInElement::insertedIntoDocument();
}

void HTMLEmbedElement::removedFromDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->removeNamedItem(m_name);

    HTMLPlugInElement::removedFromDocument();
}

void HTMLEmbedElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    HTMLPlugInElement::attributeChanged(attr, preserveDecls);

    if ((attr->name() == widthAttr || attr->name() == heightAttr) && !attr->isEmpty()) {
        Node* n = parent();
        while (n && !n->hasTagName(objectTag))
            n = n->parent();
        if (n)
            static_cast<HTMLObjectElement*>(n)->setAttribute(attr->name(), attr->value());
    }
}

bool HTMLEmbedElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == srcAttr;
}

const QualifiedName& HTMLEmbedElement::imageSourceAttributeName() const
{
    return srcAttr;
}

void HTMLEmbedElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(getAttribute(srcAttr)));
}

}