#include "dom.hpp"
#include "config.hpp"
#include "Exc.hpp"

#include <pugixml.hpp>

#include <map>
#include <set>
#include <sstream>
#include <iomanip>

using namespace svgdom;


namespace{

void skipWhitespacesAndOrComma(std::istream& s){
	bool commaSkipped = false;
	while(!s.eof()){
		if(std::isspace(s.peek())){
			s.get();
		}else if(s.peek() == ','){
			if(commaSkipped){
				break;
			}
			s.get();
			commaSkipped = true;
		}else{
			break;
		}
	}
}

std::string readTillCharOrWhitespace(std::istream& s, char c){
	std::stringstream ss;
	while(!s.eof()){
		if(std::isspace(s.peek()) || s.peek() == c || s.peek() == std::char_traits<char>::eof()){
			break;
		}
		ss << char(s.get());
	}
	return ss.str();
}

std::string readTillChar(std::istream& s, char c){
	std::stringstream ss;
	while(!s.eof()){
		if(s.peek() == c || s.peek() == std::char_traits<char>::eof()){
			break;
		}
		ss << char(s.get());
	}
	return ss.str();
}


void skipTillCharInclusive(std::istream& s, char c){
	while(!s.eof()){
		if(s.get() == c){
			break;
		}
	}
}

void skipWhitespaces(std::istream& s){
	while(!s.eof()){
		if(!std::isspace(s.peek())){
			break;
		}
		s.get();
	}
}

enum class EXmlNamespace{
	UNKNOWN,
	SVG,
	XLINK
};

const std::string DSvgNamespace = "http://www.w3.org/2000/svg";
const std::string DXlinkNamespace = "http://www.w3.org/1999/xlink";

StylePropertyValue parsePropertyValue(EStyleProperty type, std::istream& s){
	StylePropertyValue v;
	
	switch(type){
		default:
			ASSERT(false)
			break;
		case EStyleProperty::STOP_OPACITY:
		case EStyleProperty::OPACITY:
		case EStyleProperty::STROKE_OPACITY:
		case EStyleProperty::FILL_OPACITY:
			s >> v.opacity;
			if(s.fail()){
				s.clear();
			}else{
				utki::clampRange(v.opacity, real(0), real(1));
			}
			break;
		case EStyleProperty::STOP_COLOR:
		case EStyleProperty::FILL:
		case EStyleProperty::STROKE:
			v = StylePropertyValue::parsePaint(readTillChar(s, ';'));
//				TRACE(<< "paint read = " << std::hex << v.integer << std::endl)
			break;
		case EStyleProperty::STROKE_WIDTH:
			v.length = Length::parse(readTillChar(s, ';'));
//				TRACE(<< "stroke-width read = " << v.length << std::endl)
			break;
		case EStyleProperty::STROKE_LINECAP:
			{
				auto str = readTillCharOrWhitespace(s, ';');
				if(str == "butt"){
					v.strokeLineCap = EStrokeLineCap::BUTT;
				}else if(str == "round"){
					v.strokeLineCap = EStrokeLineCap::ROUND;
				}else if(str == "square"){
					v.strokeLineCap = EStrokeLineCap::SQUARE;
				}else{
					TRACE(<< "unknown strokeLineCap value:" << str << std::endl)
				}
			}
			break;
	}
	return v;
}

StylePropertyValue parsePropertyValue(EStyleProperty type, const std::string& str){
	std::istringstream s(str);
	return parsePropertyValue(type, s);
}

struct Parser{
	typedef std::map<std::string, EXmlNamespace> T_NamespaceMap;
	std::vector<T_NamespaceMap> namespaces;
	
	std::vector<EXmlNamespace> defaultNamespace;
	
	
	EXmlNamespace findNamespace(const std::string& ns){
		for(auto i = this->namespaces.rbegin(); i != this->namespaces.rend(); ++i){
			auto iter = i->find(ns);
			if(iter == i->end()){
				continue;
			}
			ASSERT(ns == iter->first)
			return iter->second;
		}
		return EXmlNamespace::UNKNOWN;
	}
	
	struct NamespaceNamePair{
		EXmlNamespace ns;
		std::string name;
	};
	
	NamespaceNamePair getNamespace(const std::string& fullName){
		NamespaceNamePair ret;
		
		auto colonIndex = fullName.find_first_of(':');
		if(colonIndex == std::string::npos){
			ret.ns = this->defaultNamespace.back();
			ret.name = fullName;
			return ret;
		}
		
		ASSERT(fullName.length() >= colonIndex + 1)
		
		ret.ns = this->findNamespace(fullName.substr(0, colonIndex));
		ret.name = fullName.substr(colonIndex + 1, fullName.length() - 1 - colonIndex);
		
		return ret;
	}
	
	std::unique_ptr<svgdom::Element> parseNode(const pugi::xml_node& n);

	void fillElement(Element& e, const pugi::xml_node& n){
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "id"){
						e.id = a.value();
					}
					break;
				default:
					break;
			}
		}
	}
	
	void fillReferencing(Referencing& e, const pugi::xml_node& n){
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::XLINK:
					if(nsn.name == "href"){
						e.iri = a.value();
					}
					break;
				default:
					break;
			}
		}
	}

	void fillRectangle(Rectangle& r, const pugi::xml_node& n){
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "x"){
						r.x = Length::parse(a.value());
					}else if(nsn.name == "y"){
						r.y = Length::parse(a.value());
					}else if(nsn.name == "width"){
						r.width = Length::parse(a.value());
					}else if(nsn.name == "height"){
						r.height = Length::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
	}
	
	void fillContainer(Container& c, const pugi::xml_node& n){
		this->fillElement(c, n);
		
		ASSERT(c.children.size() == 0)
		for(auto i = n.first_child(); !i.empty(); i = i.next_sibling()){
			if(auto res = this->parseNode(i)){
				c.children.push_back(std::move(res));
				c.children.back()->parent = &c;
			}
		}
	}
	
	void fillTransformable(Transformable& t, const pugi::xml_node& n){
		ASSERT(t.transformations.size() == 0)
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "transform"){
						t.transformations = Transformable::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
	}
	
	void fillStyleable(Styleable& s, const pugi::xml_node& n){
		ASSERT(s.styles.size() == 0)
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "style"){
						s.styles = Styleable::parse(a.value());
						break;
					}
					{
						EStyleProperty type = Styleable::stringToProperty(nsn.name);
						if(type != EStyleProperty::UNKNOWN){
							s.styles[type] = parsePropertyValue(type, a.value());
						}
					}
					break;
				default:
					break;
			}
		}
	}
	
	void fillGradient(Gradient& g, const pugi::xml_node& n){
		this->fillContainer(g, n);
		this->fillReferencing(g, n);
		this->fillStyleable(g, n);
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "spreadMethod"){
						g.spreadMethod = Gradient::stringToSpreadMethod(a.value());
					}else if(nsn.name == "gradientTransform"){
						g.transformations = Transformable::parse(a.value());
					}else if(nsn.name == "gradientUnits"){
						if(std::string("userSpaceOnUse") == a.value()){
							g.units = Gradient::EUnits::USER_SPACE_ON_USE;
						}else if(std::string("objectBoundingBox") == a.value()){
							g.units = Gradient::EUnits::OBJECT_BOUNDING_BOX;
						}
					}
					break;
				default:
					break;
			}
		}
	}
	
	void fillShape(Shape& s, const pugi::xml_node& n){
		this->fillElement(s, n);
		this->fillStyleable(s, n);
		this->fillTransformable(s, n);
	}
	
	std::unique_ptr<Gradient::StopElement> parseGradientStopElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "stop")
		
		auto ret = utki::makeUnique<Gradient::StopElement>();
		this->fillStyleable(*ret, n);
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "offset"){
						std::istringstream s(a.value());
						s >> ret->offset;
						if(!s.eof() && s.peek() == '%'){
							ret->offset /= 100;
						}
					}
					break;
				default:
					break;
			}
		}
		
		return ret;
	}
	
	std::unique_ptr<SvgElement> parseSvgElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "svg")
		
		auto ret = utki::makeUnique<SvgElement>();
		
		this->fillRectangle(*ret, n);
		this->fillContainer(*ret, n);

		return ret;
	}
	
	std::unique_ptr<GElement> parseGElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "g")
		
		auto ret = utki::makeUnique<GElement>();
		
		this->fillTransformable(*ret, n);
		this->fillStyleable(*ret, n);
		this->fillContainer(*ret, n);
		
		return ret;
	}
	
	std::unique_ptr<DefsElement> parseDefsElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "defs")
		
		auto ret = utki::makeUnique<DefsElement>();
		
		this->fillTransformable(*ret, n);
		this->fillStyleable(*ret, n);
		this->fillContainer(*ret, n);
		
		return ret;
	}
	
	std::unique_ptr<PathElement> parsePathElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "path")
		
		auto ret = utki::makeUnique<PathElement>();
		
		this->fillShape(*ret, n);

		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "d"){
						ret->path = PathElement::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
		
		return ret;
	}
	
	std::unique_ptr<RectElement> parseRectElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "rect")
		
		auto ret = utki::makeUnique<RectElement>();
		
		this->fillShape(*ret, n);
		this->fillRectangle(*ret, n);

		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "rx"){
						ret->rx = Length::parse(a.value());
					}else if(nsn.name == "ry"){
						ret->ry = Length::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
		
		return ret;
	}
	
	std::unique_ptr<EllipseElement> parseEllipseElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "ellipse")
		
		auto ret = utki::makeUnique<EllipseElement>();
		
		this->fillShape(*ret, n);

		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "cx"){
						ret->cx = Length::parse(a.value());
					}else if(nsn.name == "cy"){
						ret->cy = Length::parse(a.value());
					}else if(nsn.name == "rx"){
						ret->rx = Length::parse(a.value());
					}else if(nsn.name == "ry"){
						ret->ry = Length::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
		
		return ret;
	}
	
	std::unique_ptr<LinearGradientElement> parseLinearGradientElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "linearGradient")
		
		auto ret = utki::makeUnique<LinearGradientElement>();
		
		this->fillGradient(*ret, n);
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "x1"){
						ret->x1 = Length::parse(a.value());
					}else if(nsn.name == "y1"){
						ret->y1 = Length::parse(a.value());
					}else if(nsn.name == "x2"){
						ret->x2 = Length::parse(a.value());
					}else if(nsn.name == "y2"){
						ret->y2 = Length::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
		
		return ret;
	}
	
	std::unique_ptr<RadialGradientElement> parseRadialGradientElement(const pugi::xml_node& n){
		ASSERT(getNamespace(n.name()).ns == EXmlNamespace::SVG)
		ASSERT(getNamespace(n.name()).name == "radialGradient")
		
		auto ret = utki::makeUnique<RadialGradientElement>();
		
		this->fillGradient(*ret, n);
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto nsn = this->getNamespace(a.name());
			switch(nsn.ns){
				case EXmlNamespace::SVG:
					if(nsn.name == "cx"){
						ret->cx = Length::parse(a.value());
					}else if(nsn.name == "cy"){
						ret->cy = Length::parse(a.value());
					}else if(nsn.name == "r"){
						ret->r = Length::parse(a.value());
					}else if(nsn.name == "fx"){
						ret->fx = Length::parse(a.value());
					}else if(nsn.name == "fy"){
						ret->fy = Length::parse(a.value());
					}
					break;
				default:
					break;
			}
		}
		
		return ret;
	}
};//~class


std::unique_ptr<svgdom::Element> Parser::parseNode(const pugi::xml_node& n){
	//parse default namespace
	{
		pugi::xml_attribute dn = n.attribute("xmlns");
		if(!dn.empty()){
			if(std::string(dn.value()) == DSvgNamespace){
				this->defaultNamespace.push_back(EXmlNamespace::SVG);
			}else{
				this->defaultNamespace.push_back(EXmlNamespace::UNKNOWN);
			}
		}else{
			if(this->defaultNamespace.size() == 0){
				this->defaultNamespace.push_back(EXmlNamespace::UNKNOWN);
			}else{
				this->defaultNamespace.push_back(this->defaultNamespace.back());
			}
		}
	}
	
	//parse other namespaces
	{
		std::string xmlns = "xmlns:";
		
		this->namespaces.push_back(T_NamespaceMap());
		
		for(auto a = n.first_attribute(); !a.empty(); a = a.next_attribute()){
			auto attr = std::string(a.name());
			
			if(attr.substr(0, xmlns.length()) != xmlns){
				continue;
			}
			
			ASSERT(attr.length() >= xmlns.length())
			auto ns = attr.substr(xmlns.length(), attr.length() - xmlns.length());
			
			if(DSvgNamespace == a.value()){
				this->namespaces.back()[ns] = EXmlNamespace::SVG;
			}else if(DXlinkNamespace == a.value()){
				this->namespaces.back()[ns] = EXmlNamespace::XLINK;
			}
		}
	}
	
	utki::ScopeExit scopeExit([this](){
		ASSERT(this->namespaces.size() > 0)
		this->namespaces.pop_back();
		ASSERT(this->defaultNamespace.size() > 0)
		this->defaultNamespace.pop_back();
	});
	
	auto nsn = getNamespace(n.name());
	switch(nsn.ns){
		case EXmlNamespace::SVG:
			if(nsn.name == "svg"){
				return this->parseSvgElement(n);
			}else if(nsn.name == "g"){
				return this->parseGElement(n);
			}else if(nsn.name == "defs"){
				return this->parseDefsElement(n);
			}else if(nsn.name == "path"){
				return this->parsePathElement(n);
			}else if(nsn.name == "linearGradient"){
				return this->parseLinearGradientElement(n);
			}else if(nsn.name == "radialGradient"){
				return this->parseRadialGradientElement(n);
			}else if(nsn.name == "stop"){
				return this->parseGradientStopElement(n);
			}else if(nsn.name == "rect"){
				return this->parseRectElement(n);
			}else if(nsn.name == "ellipse"){
				return this->parseEllipseElement(n);
			}
			
			break;
		default:
			//unknown namespace, ignore
			break;
	}
	
	return nullptr;
}

void resolveReferences(Element& e, SvgElement& svg){
	if(auto r = dynamic_cast<Referencing*>(&e)){
		if(r->iri.length() != 0 && r->iri[0] == '#'){
			r->ref = svg.findById(r->iri.substr(1, r->iri.length() - 1));
			if(r->ref){
				r->iri.clear();
			}
		}
	}
	
	if(auto s = dynamic_cast<Styleable*>(&e)){
		for(auto& p : s->styles){
			if(p.second.isUrl()){
				if(p.second.str.length() != 0 && p.second.str[0] == '#'){
					p.second.url = svg.findById(p.second.str.substr(1, p.second.str.length() - 1));
				}
			}
		}
	}
	
	if(auto container = dynamic_cast<Container*>(&e)){
		for(auto& c : container->children){
			resolveReferences(*c, svg);
		}
	}
}

}//~namespace



std::unique_ptr<SvgElement> svgdom::load(const papki::File& f){
	pugi::xml_document doc;
	{
		auto fileContents = f.loadWholeFileIntoMemory();
		if(doc.load_buffer(&*fileContents.begin(), fileContents.size()).status != pugi::xml_parse_status::status_ok){
			TRACE(<< "svgdom::load(): loading XML document failed!" << std::endl)
			return nullptr;
		}
	}
	
	Parser parser;
	
	//return first node which is successfully parsed
	for(auto n = doc.first_child(); !n.empty(); n = n.next_sibling()){
		auto element = parser.parseNode(doc.first_child());
	
		auto ret = std::unique_ptr<SvgElement>(dynamic_cast<SvgElement*>(element.release()));
		if(ret){
			resolveReferences(*ret, *ret);
			return ret;
		}
	}
	return nullptr;
}



Length Length::parse(const std::string& str) {
	Length ret;

	std::istringstream ss(str);
	
	ss >> std::skipws;
	
	ss >> ret.value;
	
	std::string u;
	
	ss >> std::setw(2) >> u >> std::setw(0);
	
	if(u.length() == 0){
		ret.unit = Length::EUnit::NUMBER;
	}else if(u == "%"){
		ret.unit = Length::EUnit::PERCENT;
	}else if(u == "em"){
		ret.unit = Length::EUnit::EM;
	}else if(u == "ex"){
		ret.unit = Length::EUnit::EX;
	}else if(u == "px"){
		ret.unit = Length::EUnit::PX;
	}else if(u == "cm"){
		ret.unit = Length::EUnit::CM;
	}else if(u == "in"){
		ret.unit = Length::EUnit::IN;
	}else if(u == "pt"){
		ret.unit = Length::EUnit::PT;
	}else if(u == "pc"){
		ret.unit = Length::EUnit::PC;
	}else{
		ret.unit = Length::EUnit::UNKNOWN;
	}
	
	return ret;
}



std::ostream& operator<<(std::ostream& s, const Length& l){
	s << l.value;
	
	switch(l.unit){
		case Length::EUnit::UNKNOWN:
		case Length::EUnit::NUMBER:
		default:
			break;
		case Length::EUnit::PERCENT:
			s << "%";
			break;
		case Length::EUnit::EM:
			s << "em";
			break;
		case Length::EUnit::EX:
			s << "ex";
			break;
		case Length::EUnit::PX:
			s << "px";
			break;
		case Length::EUnit::CM:
			s << "cm";
			break;
		case Length::EUnit::IN:
			s << "in";
			break;
		case Length::EUnit::PT:
			s << "pt";
			break;
		case Length::EUnit::PC:
			s << "pc";
			break;
	}
	
	return s;
}



void Element::attribsToStream(std::ostream& s) const{
	if(this->id.length() != 0){
		s << " id=\"" << this->id << "\"";
	}
}



void Rectangle::attribsToStream(std::ostream& s)const{
	if(this->x.value != 0){
		s << " x=\"" << this->x << "\"";
	}
	
	if(this->y.value != 0){
		s << " y=\"" << this->y << "\"";
	}
	
	if(this->width.value != 100 || this->width.unit != Length::EUnit::PERCENT){ //if width is not 100% (default value)
		s << " width=\"" << this->width << "\"";
	}
	
	if(this->height.value != 100 || this->height.unit != Length::EUnit::PERCENT){ //if height is not 100% (default value)
		s << " height=\"" << this->height << "\"";
	}
}


namespace{

std::string indentStr(unsigned indent){
	std::string ind;

	std::stringstream ss;
	for(unsigned i = 0; i != indent; ++i){
		ss << "\t";
	}
	return ss.str();
}

}//~namespace


void SvgElement::toStream(std::ostream& s, unsigned indent) const{
	auto ind = indentStr(indent);
	
	s << ind << "<svg";
	this->Container::attribsToStream(s);
	this->Rectangle::attribsToStream(s);
	
	if(this->children.size() == 0){
		s << "/>";
	}else{
		s << ">" << std::endl;
		this->childrenToStream(s, indent + 1);
		s << ind << "</svg>";
	}
	s << std::endl;
}

void Container::childrenToStream(std::ostream& s, unsigned indent) const{
	for(auto& e : this->children){
		e->toStream(s, indent);
	}
}

std::string Element::toString() const{
	std::stringstream s;
	this->toStream(s, 0);
	return s.str();
}

void GElement::toStream(std::ostream& s, unsigned indent) const{
	auto ind = indentStr(indent);
	
	s << ind << "<g";
	this->Container::attribsToStream(s);
	this->Transformable::attribsToStream(s);
	this->Styleable::attribsToStream(s);
	
	if(this->children.size() == 0){
		s << "/>";
	}else{
		s << ">" << std::endl;
		this->childrenToStream(s, indent + 1);
		s << ind << "</g>";
	}
	s << std::endl;
}

void DefsElement::toStream(std::ostream& s, unsigned indent) const{
	auto ind = indentStr(indent);
	
	s << ind << "<defs";
	this->Container::attribsToStream(s);
	this->Transformable::attribsToStream(s);
	this->Styleable::attribsToStream(s);
	
	if(this->children.size() == 0){
		s << "/>";
	}else{
		s << ">" << std::endl;
		this->childrenToStream(s, indent + 1);
		s << ind << "</defs>";
	}
	s << std::endl;
}


void Styleable::attribsToStream(std::ostream& s) const{
	if(this->styles.size() == 0){
		return;
	}
	
	s << " style=\"";
	
	bool isFirst = true;
	
	for(auto& st : this->styles){
		if(isFirst){
			isFirst = false;
		}else{
			s << "; ";
		}
		
		ASSERT(st.first != EStyleProperty::UNKNOWN)
		
		s << propertyToString(st.first) << ":";
		
		switch(st.first){
			default:
				ASSERT(false)
				break;
			case EStyleProperty::STOP_OPACITY:
			case EStyleProperty::OPACITY:
			case EStyleProperty::STROKE_OPACITY:
			case EStyleProperty::FILL_OPACITY:
				s << st.second.opacity;
				break;
			case EStyleProperty::STOP_COLOR:
			case EStyleProperty::FILL:
			case EStyleProperty::STROKE:
				s << st.second.paintToString();
				break;
			case EStyleProperty::STROKE_WIDTH:
				s << st.second.length;
				break;
			case EStyleProperty::STROKE_LINECAP:
				switch(st.second.strokeLineCap){
					default:
						ASSERT(false)
						break;
					case EStrokeLineCap::BUTT:
						s << "butt";
						break;
					case EStrokeLineCap::ROUND:
						s << "round";
						break;
					case EStrokeLineCap::SQUARE:
						s << "square";
						break;
				}
				break;
		}
	}
	
	s << "\"";
}

void Transformable::transformationsToStream(std::ostream& s) const {
	bool isFirst = true;
	
	for(auto& t : this->transformations){
		if(isFirst){
			isFirst = false;
		}else{
			s << " ";
		}
		
		switch(t.type){
			default:
				ASSERT(false)
				break;
			case Transformation::EType::MATRIX:
				s << "matrix(" << t.a << "," << t.b << "," << t.c << "," << t.d << "," << t.e << "," << t.f << ")";
				break;
			case Transformation::EType::TRANSLATE:
				s << "translate(" << t.x;
				if(t.y != 0){
					s << "," << t.y;
				}
				s << ")";
				break;
			case Transformation::EType::SCALE:
				s << "scale(" << t.x;
				if(t.x != t.y){
					s << "," << t.y;
				}
				s << ")";
				break;
			case Transformation::EType::ROTATE:
				s << "rotate(" << t.angle;
				if(t.x != 0 || t.y != 0){
					s << "," << t.x << "," << t.y;
				}
				s << ")";
				break;
			case Transformation::EType::SKEWX:
				s << "skewX(" << t.angle << ")";
				break;
			case Transformation::EType::SKEWY:
				s << "skewY(" << t.angle << ")";
				break;
		}
	}
}

void Transformable::attribsToStream(std::ostream& s) const{
	if(this->transformations.size() == 0){
		return;
	}
	
	s << " transform=\"";
	this->transformationsToStream(s);
	s << "\"";
}

decltype(Transformable::transformations) Transformable::parse(const std::string& str){
	std::istringstream s(str);
	
	s >> std::skipws;
	s >> std::setfill(' ');
	
	decltype(Transformable::transformations) ret;
	
	while(!s.eof()){
		std::string transform = readTillCharOrWhitespace(s, '(');
		
		Transformation t;
		
		if(transform == "matrix"){
			t.type = Transformation::EType::MATRIX;
		}else if(transform == "translate"){
			t.type = Transformation::EType::TRANSLATE;
		}else if(transform == "scale"){
			t.type = Transformation::EType::SCALE;
		}else if(transform == "rotate"){
			t.type = Transformation::EType::ROTATE;
		}else if(transform == "skewX"){
			t.type = Transformation::EType::SKEWX;
		}else if(transform == "skewY"){
			t.type = Transformation::EType::SKEWY;
		}else{
			return ret;//unknown transformation, stop parsing
		}
		
		{
			std::string str;
			s >> std::setw(1) >> str >> std::setw(0);
			if(str != "("){
				return ret;//expected (
			}
		}
		
		switch(t.type){
			default:
				ASSERT(false)
				break;
			case Transformation::EType::MATRIX:
				s >> t.a;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.b;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.c;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.d;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.e;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.f;
				if(s.fail()){
					return ret;
				}
				break;
			case Transformation::EType::TRANSLATE:
				s >> t.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.y;
				if(s.fail()){
					s.clear();
					t.y = 0;
				}
				break;
			case Transformation::EType::SCALE:
				s >> t.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.y;
				if(s.fail()){
					s.clear();
					t.y = t.x;
				}
				break;
			case Transformation::EType::ROTATE:
				s >> t.angle;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> t.x;
				if(s.fail()){
					s.clear();
					t.x = 0;
					t.y = 0;
				}else{
					skipWhitespacesAndOrComma(s);
					s >> t.y;
					if(s.fail()){
						return ret;//malformed rotate transformation
					}
				}
				break;
			case Transformation::EType::SKEWY:
			case Transformation::EType::SKEWX:
				s >> t.angle;
				if(s.fail()){
					return ret;
				}
				break;
		}
		
		{
			std::string str;
			s >> std::setw(1) >> str >> std::setw(0);
			if(str != ")"){
				return ret;//expected )
			}
		}
		
		ret.push_back(t);
		
		skipWhitespacesAndOrComma(s);
	}
	
	return ret;
}

EStyleProperty Styleable::stringToProperty(std::string str){
	if(str == "fill"){
		return EStyleProperty::FILL;
	}else if(str == "fill-opacity"){
		return EStyleProperty::FILL_OPACITY;
	}else if(str == "stroke"){
		return EStyleProperty::STROKE;
	}else if(str == "stroke-width"){
		return EStyleProperty::STROKE_WIDTH;
	}else if(str == "stroke-linecap"){
		return EStyleProperty::STROKE_LINECAP;
	}else if(str == "stroke-opacity"){
		return EStyleProperty::STROKE_OPACITY;
	}else if(str == "opacity"){
		return EStyleProperty::OPACITY;
	}else if(str == "stop-opacity"){
		return EStyleProperty::STOP_OPACITY;
	}else if(str == "stop-color"){
		return EStyleProperty::STOP_COLOR;
	}
	
	return EStyleProperty::UNKNOWN;
}

std::string Styleable::propertyToString(EStyleProperty p){
	switch(p){
		default:
			ASSERT(false)
			return "";
		case EStyleProperty::FILL:
			return "fill";
		case EStyleProperty::FILL_OPACITY:
			return "fill-opacity";
		case EStyleProperty::STROKE:
			return "stroke";
		case EStyleProperty::STROKE_WIDTH:
			return "stroke-width";
		case EStyleProperty::STROKE_LINECAP:
			return "stroke-linecap";
		case EStyleProperty::STROKE_OPACITY:
			return "stroke-opacity";
		case EStyleProperty::OPACITY:
			return "opacity";
		case EStyleProperty::STOP_OPACITY:
			return "stop-opacity";
		case EStyleProperty::STOP_COLOR:
			return "stop-color";
	}
}


decltype(Styleable::styles) Styleable::parse(const std::string& str){
	std::stringstream s(str);
	
	s >> std::skipws;
	s >> std::setfill(' ');
	
	decltype(Styleable::styles) ret;
	
	while(!s.eof()){
		skipWhitespaces(s);
		std::string property = readTillCharOrWhitespace(s, ':');
		
		EStyleProperty type = Styleable::stringToProperty(property);
		
		if(type == EStyleProperty::UNKNOWN){
			//unknown style property, skip it
			TRACE(<< "Unknown style property: " << property << std::endl)
			skipTillCharInclusive(s, ';');
			continue;
		}
		
		{
			std::string str;
			s >> std::setw(1) >> str >> std::setw(0);
			if(str != ":"){
				return ret;//expected colon
			}
		}
		
		skipWhitespaces(s);
		
		StylePropertyValue v = parsePropertyValue(type, s);
		
		skipWhitespaces(s);
		
		if(!s.eof() && s.get() != ';'){
			return ret;//expected semicolon
		}
		
		ret[type] = std::move(v);
	}
	
	return ret;
}

namespace{
const std::map<std::string, std::uint32_t> colorNames = {
	{"aliceblue", 0xfff8f0},
	{"antiquewhite", 0xd7ebfa},
	{"aqua", 0xffff00},
	{"aquamarine", 0xd4ff7f},
	{"azure", 0xfffff0},
	{"beige", 0xdcf5f5},
	{"bisque", 0xc4e4ff},
	{"black", 0x0},
	{"blanchedalmond", 0xcdebff},
	{"blue", 0xff0000},
	{"blueviolet", 0xe22b8a},
	{"brown", 0x2a2aa5},
	{"burlywood", 0x87b8de},
	{"cadetblue", 0xa09e5f},
	{"chartreuse", 0xff7f},
	{"chocolate", 0x1e69d2},
	{"coral", 0x507fff},
	{"cornflowerblue", 0xed9564},
	{"cornsilk", 0xdcf8ff},
	{"crimson", 0x3c14dc},
	{"cyan", 0xffff00},
	{"darkblue", 0x8b0000},
	{"darkcyan", 0x8b8b00},
	{"darkgoldenrod", 0xb86b8},
	{"darkgray", 0xa9a9a9},
	{"darkgreen", 0x6400},
	{"darkgrey", 0xa9a9a9},
	{"darkkhaki", 0x6bb7bd},
	{"darkmagenta", 0x8b008b},
	{"darkolivegreen", 0x2f6b55},
	{"darkorange", 0x8cff},
	{"darkorchid", 0xcc3299},
	{"darkred", 0x8b},
	{"darksalmon", 0x7a96e9},
	{"darkseagreen", 0x8fbc8f},
	{"darkslateblue", 0x8b3d48},
	{"darkslategray", 0x4f4f2f},
	{"darkslategrey", 0x4f4f2f},
	{"darkturquoise", 0xd1ce00},
	{"darkviolet", 0xd30094},
	{"deeppink", 0x9314ff},
	{"deepskyblue", 0xffbf00},
	{"dimgray", 0x696969},
	{"dimgrey", 0x696969},
	{"dodgerblue", 0xff901e},
	{"firebrick", 0x2222b2},
	{"floralwhite", 0xf0faff},
	{"forestgreen", 0x228b22},
	{"fuchsia", 0xff00ff},
	{"gainsboro", 0xdcdcdc},
	{"ghostwhite", 0xfff8f8},
	{"gold", 0xd7ff},
	{"goldenrod", 0x20a5da},
	{"gray", 0x808080},
	{"grey", 0x808080},
	{"green", 0x8000},
	{"greenyellow", 0x2fffad},
	{"honeydew", 0xf0fff0},
	{"hotpink", 0xb469ff},
	{"indianred", 0x5c5ccd},
	{"indigo", 0x82004b},
	{"ivory", 0xf0ffff},
	{"khaki", 0x8ce6f0},
	{"lavender", 0xfae6e6},
	{"lavenderblush", 0xf5f0ff},
	{"lawngreen", 0xfc7c},
	{"lemonchiffon", 0xcdfaff},
	{"lightblue", 0xe6d8ad},
	{"lightcoral", 0x8080f0},
	{"lightcyan", 0xffffe0},
	{"lightgoldenrodyellow", 0xd2fafa},
	{"lightgray", 0xd3d3d3},
	{"lightgreen", 0x90ee90},
	{"lightgrey", 0xd3d3d3},
	{"lightpink", 0xc1b6ff},
	{"lightsalmon", 0x7aa0ff},
	{"lightseagreen", 0xaab220},
	{"lightskyblue", 0xface87},
	{"lightslategray", 0x998877},
	{"lightslategrey", 0x998877},
	{"lightsteelblue", 0xdec4b0},
	{"lightyellow", 0xe0ffff},
	{"lime", 0xff00},
	{"limegreen", 0x32cd32},
	{"linen", 0xe6f0fa},
	{"magenta", 0xff00ff},
	{"maroon", 0x80},
	{"mediumaquamarine", 0xaacd66},
	{"mediumblue", 0xcd0000},
	{"mediumorchid", 0xd355ba},
	{"mediumpurple", 0xdb7093},
	{"mediumseagreen", 0x71b33c},
	{"mediumslateblue", 0xee687b},
	{"mediumspringgreen", 0x9afa00},
	{"mediumturquoise", 0xccd148},
	{"mediumvioletred", 0x8515c7},
	{"midnightblue", 0x701919},
	{"mintcream", 0xfafff5},
	{"mistyrose", 0xe1e4ff},
	{"moccasin", 0xb5e4ff},
	{"navajowhite", 0xaddeff},
	{"navy", 0x800000},
	{"oldlace", 0xe6f5fd},
	{"olive", 0x8080},
	{"olivedrab", 0x238e6b},
	{"orange", 0xa5ff},
	{"orangered", 0x45ff},
	{"orchid", 0xd670da},
	{"palegoldenrod", 0xaae8ee},
	{"palegreen", 0x98fb98},
	{"paleturquoise", 0xeeeeaf},
	{"palevioletred", 0x9370db},
	{"papayawhip", 0xd5efff},
	{"peachpuff", 0xb9daff},
	{"peru", 0x3f85cd},
	{"pink", 0xcbc0ff},
	{"plum", 0xdda0dd},
	{"powderblue", 0xe6e0b0},
	{"purple", 0x800080},
	{"red", 0xff},
	{"rosybrown", 0x8f8fbc},
	{"royalblue", 0xe16941},
	{"saddlebrown", 0x13458b},
	{"salmon", 0x7280fa},
	{"sandybrown", 0x60a4f4},
	{"seagreen", 0x578b2e},
	{"seashell", 0xeef5ff},
	{"sienna", 0x2d52a0},
	{"silver", 0xc0c0c0},
	{"skyblue", 0xebce87},
	{"slateblue", 0xcd5a6a},
	{"slategray", 0x908070},
	{"slategrey", 0x908070},
	{"snow", 0xfafaff},
	{"springgreen", 0x7fff00},
	{"steelblue", 0xb48246},
	{"tan", 0x8cb4d2},
	{"teal", 0x808000},
	{"thistle", 0xd8bfd8},
	{"tomato", 0x4763ff},
	{"turquoise", 0xd0e040},
	{"violet", 0xee82ee},
	{"wheat", 0xb3def5},
	{"white", 0xffffff},
	{"whitesmoke", 0xf5f5f5},
	{"yellow", 0xffff},
	{"yellowgreen", 0x32cd9a}
};

}//~namespace


StylePropertyValue StylePropertyValue::parsePaint(const std::string& str){
	StylePropertyValue ret;
	
	if(str.size() == 0){
		ret.rule = StylePropertyValue::ERule::NONE;
		return ret;
	}
	
	ASSERT(!std::isspace(str[0])) //leading spaces should be skept already	
	
	//check if 'none'
	{
		const std::string cmp = "none";
		if(cmp == str.substr(0, cmp.length())){
			ret.rule = StylePropertyValue::ERule::NONE;
			return ret;
		}
	}
	
	//check if # notation
	if(str[0] == '#'){
		std::istringstream s(str.substr(1, 6));
		
		std::array<std::uint8_t, 6> d;
		unsigned numDigits = 0;
		for(auto i = d.begin(); i != d.end(); ++i, ++numDigits){
			char c = s.get();
			if('0' <= c && c <= '9'){
				(*i) = c - '0';
			}else if('a' <= c && c <= 'f'){
				(*i) = 10 + c - 'a';
			}else if('A' <= c && c <= 'F'){
				(*i) = 10 + c - 'A';
			}else{
				break;
			}
			
			ASSERT(((*i) & 0xf0) == 0) //only one hex digit
		}
		switch(numDigits){
			case 3:
				ret.color = (std::uint32_t(d[0]) << 4) | (std::uint32_t(d[0]))
						| (std::uint32_t(d[1]) << 12) | (std::uint32_t(d[1]) << 8)
						| (std::uint32_t(d[2]) << 20) | (std::uint32_t(d[2]) << 16);
				break;
			case 6:
				ret.color = (std::uint32_t(d[0]) << 4) | (std::uint32_t(d[1]))
						| (std::uint32_t(d[2]) << 12) | (std::uint32_t(d[3]) << 8)
						| (std::uint32_t(d[4]) << 20) | (std::uint32_t(d[5]) << 16);
				break;
			default:
				ret.rule = StylePropertyValue::ERule::NONE;
				break;
		}
		
//		TRACE(<< "# color read = " << std::hex << ret.integer << std::endl)
		return ret;
	}
	
	//check if rgb() or RGB() notation
	{
		const std::string rgb = "rgb(";
		if(rgb == str.substr(0, rgb.length())){
			std::istringstream s(str);
			
			std::string tmpStr;
			
			s >> std::setw(rgb.length()) >> tmpStr >> std::setw(0);
			ASSERT(tmpStr == rgb)
			
			std::uint32_t r, g, b;
			
			skipWhitespaces(s);
			s >> r;
			skipWhitespacesAndOrComma(s);
			s >> g;
			skipWhitespacesAndOrComma(s);
			s >> b;
			skipWhitespaces(s);
			
			if(s.get() == ')'){
				ret.color = r | (g << 8) | (b << 16);
			}
			return ret;
		}
	}
	
	{
		std::string url = "url(";
		if(url == str.substr(0, url.length())){
			std::istringstream s(str);
			
			std::string tmpStr;
			s >> std::setw(url.length()) >> tmpStr >> std::setw(0);
			ASSERT(tmpStr == url)
			
			skipWhitespaces(s);
			tmpStr = readTillCharOrWhitespace(s, ')');
			
			skipWhitespaces(s);
			if(s.get() == ')'){
				ret.str = tmpStr;
				ret.rule = StylePropertyValue::ERule::URL;
				ret.url = nullptr;
				return ret;
			}
		}
	}
	
	//check if color name
	{
		std::istringstream s(str);
		std::string name;
		s >> name;
		
		auto i = colorNames.find(name);
		if(i != colorNames.end()){
			ASSERT(i->first == name)
			ret.str = name;
			ret.color = i->second;
			return ret;
		}
	}
	
	return ret;
}

std::string StylePropertyValue::paintToString()const{
	switch(this->rule){
		default:
		case ERule::NONE:
			return "none";
		case ERule::INHERIT:
			return "inherit";
		case ERule::NORMAL:
			if(this->str.size() == 0){
				//it is a # notation

				std::stringstream s;
				s << std::hex;
				s << "#";
				s << ((this->color >> 4) & 0xf);
				s << ((this->color) & 0xf);
				s << ((this->color >> 12) & 0xf);
				s << ((this->color >> 8) & 0xf);
				s << ((this->color >> 20) & 0xf);
				s << ((this->color >> 16) & 0xf);
				return s.str();
			}else{
				return this->str;
			}
	}
}

void PathElement::toStream(std::ostream& s, unsigned indent) const{
	auto ind = indentStr(indent);
	
	s << ind << "<path";

	this->attribsToStream(s);
	
	s << "/>";
	s << std::endl;
}

void Shape::attribsToStream(std::ostream& s) const {
	this->Element::attribsToStream(s);
	this->Transformable::attribsToStream(s);
	this->Styleable::attribsToStream(s);
}


void PathElement::attribsToStream(std::ostream& s) const{
	this->Shape::attribsToStream(s);
	
	if(this->path.size() == 0){
		return;
	}
	
	s << " d=\"";
	
	Step::EType curType = Step::EType::UNKNOWN;
	
	for(auto& step : this->path){
		if(curType == step.type){
			s << " ";
		}else{
			s << Step::typeToChar(step.type);
			curType = step.type;
		}
		
		switch(step.type){
			case Step::EType::MOVE_ABS:
			case Step::EType::MOVE_REL:
			case Step::EType::LINE_ABS:
			case Step::EType::LINE_REL:
				s << step.x;
				s << ",";
				s << step.y;
				break;
			case Step::EType::CLOSE:
				break;
			case Step::EType::HORIZONTAL_LINE_ABS:
			case Step::EType::HORIZONTAL_LINE_REL:
				s << step.x;
				break;
			case Step::EType::VERTICAL_LINE_ABS:
			case Step::EType::VERTICAL_LINE_REL:
				s << step.y;
				break;
			case Step::EType::CUBIC_ABS:
			case Step::EType::CUBIC_REL:
				s << step.x1;
				s << ",";
				s << step.y1;
				s << " ";
				s << step.x2;
				s << ",";
				s << step.y2;
				s << " ";
				s << step.x;
				s << ",";
				s << step.y;
				break;
			case Step::EType::CUBIC_SMOOTH_ABS:
			case Step::EType::CUBIC_SMOOTH_REL:
				s << step.x2;
				s << ",";
				s << step.y2;
				s << " ";
				s << step.x;
				s << ",";
				s << step.y;
				break;
			case Step::EType::QUADRATIC_ABS:
			case Step::EType::QUADRATIC_REL:
				s << step.x1;
				s << ",";
				s << step.y1;
				s << " ";
				s << step.x;
				s << ",";
				s << step.y;
				break;
			case Step::EType::QUADRATIC_SMOOTH_ABS:
			case Step::EType::QUADRATIC_SMOOTH_REL:
				s << step.x;
				s << ",";
				s << step.y;
				break;
			case Step::EType::ARC_ABS:
			case Step::EType::ARC_REL:
				s << step.rx;
				s << ",";
				s << step.ry;
				s << " ";
				s << step.xAxisRotation;
				s << " ";
				s << (step.flags.largeArc ? "1" : "0");
				s << ",";
				s << (step.flags.sweep ? "1" : "0");
				s << " ";
				s << step.x;
				s << ",";
				s << step.y;
				break;
			default:
				ASSERT(false)
				break;
		}
	}
	
	s << "\"";
}

char PathElement::Step::typeToChar(EType t){
	switch(t){
		case Step::EType::MOVE_ABS:
			return 'M';
		case Step::EType::MOVE_REL:
			return 'm';
		case Step::EType::LINE_ABS:
			return 'L';
		case Step::EType::LINE_REL:
			return 'l';
		case Step::EType::CLOSE:
			return 'z';
		case Step::EType::HORIZONTAL_LINE_ABS:
			return 'H';
		case Step::EType::HORIZONTAL_LINE_REL:
			return 'h';
		case Step::EType::VERTICAL_LINE_ABS:
			return 'V';
		case Step::EType::VERTICAL_LINE_REL:
			return 'v';
		case Step::EType::CUBIC_ABS:
			return 'C';
		case Step::EType::CUBIC_REL:
			return 'c';
		case Step::EType::CUBIC_SMOOTH_ABS:
			return 'S';
		case Step::EType::CUBIC_SMOOTH_REL:
			return 's';
		case Step::EType::QUADRATIC_ABS:
			return 'Q';
		case Step::EType::QUADRATIC_REL:
			return 'q';
		case Step::EType::QUADRATIC_SMOOTH_ABS:
			return 'T';
		case Step::EType::QUADRATIC_SMOOTH_REL:
			return 't';
		case Step::EType::ARC_ABS:
			return 'A';
		case Step::EType::ARC_REL:
			return 'a';
		default:
			ASSERT(false)
			return ' ';
	}
}


PathElement::Step::EType PathElement::Step::charToType(char c){
	switch(c){
		case 'M':
			return Step::EType::MOVE_ABS;
		case 'm':
			return Step::EType::MOVE_REL;
		case 'z':
		case 'Z':
			return Step::EType::CLOSE;
		case 'L':
			return Step::EType::LINE_ABS;
		case 'l':
			return Step::EType::LINE_REL;
		case 'H':
			return Step::EType::HORIZONTAL_LINE_ABS;
		case 'h':
			return Step::EType::HORIZONTAL_LINE_REL;
		case 'V':
			return Step::EType::VERTICAL_LINE_ABS;
		case 'v':
			return Step::EType::VERTICAL_LINE_REL;
		case 'C':
			return Step::EType::CUBIC_ABS;
		case 'c':
			return Step::EType::CUBIC_REL;
		case 'S':
			return Step::EType::CUBIC_SMOOTH_ABS;
		case 's':
			return Step::EType::CUBIC_SMOOTH_REL;
		case 'Q':
			return Step::EType::QUADRATIC_ABS;
		case 'q':
			return Step::EType::QUADRATIC_REL;
		case 'T':
			return Step::EType::QUADRATIC_SMOOTH_ABS;
		case 't':
			return Step::EType::QUADRATIC_SMOOTH_REL;
		case 'A':
			return Step::EType::ARC_ABS;
		case 'a':
			return Step::EType::ARC_REL;
		default:
			return Step::EType::UNKNOWN;
	}
}


decltype(PathElement::path) PathElement::parse(const std::string& str){
	decltype(PathElement::path) ret;
	
	std::istringstream s(str);
	s >> std::skipws;
	
	skipWhitespaces(s);
	
	Step::EType curType = Step::EType::MOVE_ABS;
	
	while(!s.eof()){
		ASSERT(!std::isspace(s.peek()))//spaces should be skept
		
		{
			auto t = Step::charToType(s.peek());
			if(t != Step::EType::UNKNOWN){
				curType = t;
				s.get();
			}
		}
		
		skipWhitespaces(s);
		
		Step step;
		step.type = curType;
		
		switch(step.type){
			case Step::EType::MOVE_ABS:
			case Step::EType::MOVE_REL:
			case Step::EType::LINE_ABS:
			case Step::EType::LINE_REL:
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::CLOSE:
				break;
			case Step::EType::HORIZONTAL_LINE_ABS:
			case Step::EType::HORIZONTAL_LINE_REL:
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::VERTICAL_LINE_ABS:
			case Step::EType::VERTICAL_LINE_REL:
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::CUBIC_ABS:
			case Step::EType::CUBIC_REL:
				s >> step.x1;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y1;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.x2;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y2;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::CUBIC_SMOOTH_ABS:
			case Step::EType::CUBIC_SMOOTH_REL:
				s >> step.x2;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y2;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::QUADRATIC_ABS:
			case Step::EType::QUADRATIC_REL:
				s >> step.x1;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y1;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::QUADRATIC_SMOOTH_ABS:
			case Step::EType::QUADRATIC_SMOOTH_REL:
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			case Step::EType::ARC_ABS:
			case Step::EType::ARC_REL:
				s >> step.rx;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.ry;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.xAxisRotation;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				{
					unsigned f;
					s >> f;
					if(s.fail()){
						return ret;
					}
					step.flags.largeArc = (f != 0);
				}
				skipWhitespacesAndOrComma(s);
				{
					unsigned f;
					s >> f;
					if(s.fail()){
						return ret;
					}
					step.flags.sweep = (f != 0);
				}
				skipWhitespacesAndOrComma(s);
				s >> step.x;
				if(s.fail()){
					return ret;
				}
				skipWhitespacesAndOrComma(s);
				s >> step.y;
				if(s.fail()){
					return ret;
				}
				break;
			default:
				ASSERT(false)
				break;
		}
		
		ret.push_back(step);
		
		skipWhitespacesAndOrComma(s);
	}
	
	return ret;
}

void Container::render(Renderer& renderer) const{
	for(auto& e : this->children){
		e->render(renderer);
	}
}

void PathElement::render(Renderer& renderer) const{
	renderer.render(*this);
}

void RectElement::render(Renderer& renderer) const {
	renderer.render(*this);
}

void EllipseElement::render(Renderer& renderer) const {
	renderer.render(*this);
}


void GElement::render(Renderer& renderer) const{
	renderer.render(*this);
}


void SvgElement::render(Renderer& renderer) const{
	renderer.render(*this);
}

Rgb StylePropertyValue::getRgb() const{
	auto c = this->color;
	
	Rgb ret;
	
	ret.r = real(c & 0xff) / real(0xff);
	ret.g = real((c >> 8) & 0xff) / real(0xff);
	ret.b = real((c >> 16) & 0xff) / real(0xff);
	
	return ret;
}

namespace{
const std::set<EStyleProperty> nonInheritedStyleProoperties = {
	EStyleProperty::OPACITY
	//TODO: check if there are other non-inherited properties
};
}//~namespace

const StylePropertyValue* Element::getStyleProperty(EStyleProperty property, bool explicitInherit) const{
	if(auto styleable = dynamic_cast<const Styleable*>(this)){
		auto i = styleable->styles.find(property);
		if(i != styleable->styles.end()){
			if(i->second.rule == StylePropertyValue::ERule::INHERIT){
				explicitInherit = true;
			}else{
				return &i->second;
			}
		}
	}
	
	if(!explicitInherit && nonInheritedStyleProoperties.find(property) != nonInheritedStyleProoperties.end()){
		return nullptr;
	}
	
	if(!this->parent){
		return nullptr;
	}
	
	return this->parent->getStyleProperty(property, explicitInherit);
}

Gradient::ESpreadMethod Gradient::stringToSpreadMethod(const std::string& str) {
	if(str == "pad"){
		return ESpreadMethod::PAD;
	}else if(str == "reflect"){
		return ESpreadMethod::REFLECT;
	}else if(str == "repeat"){
		return ESpreadMethod::REPEAT;
	}
	return ESpreadMethod::DEFAULT;
}

std::string Gradient::spreadMethodToString(ESpreadMethod sm) {
	switch(sm){
		default:
			ASSERT_INFO(false, "sm = " << unsigned(sm))
		case ESpreadMethod::DEFAULT:
			return "";
		case ESpreadMethod::PAD:
			return "pad";
		case ESpreadMethod::REFLECT:
			return "reflect";
		case ESpreadMethod::REPEAT:
			return "repeat";
	}
}

void LinearGradientElement::toStream(std::ostream& s, unsigned indent) const {
	auto ind = indentStr(indent);
	
	s << ind << "<linearGradient";
	this->Gradient::attribsToStream(s);
	
	if(this->x1.unit != Length::EUnit::PERCENT || this->x1.value != 0){
		s << " x1=\"" << this->x1 << "\"";
	}
	
	if(this->y1.unit != Length::EUnit::PERCENT || this->y1.value != 0){
		s << " y1=\"" << this->y1 << "\"";
	}
	
	if(this->x2.unit != Length::EUnit::PERCENT || this->x2.value != 100){
		s << " x2=\"" << this->x2 << "\"";
	}
	
	if(this->y2.unit != Length::EUnit::PERCENT || this->y2.value != 0){
		s << " y2=\"" << this->y2 << "\"";
	}
	
	if(this->children.size() == 0){
		s << "/>";
	}else{
		s << ">" << std::endl;
		this->childrenToStream(s, indent + 1);
		s << ind << "</linearGradient>";
	}
	s << std::endl;
}

void RadialGradientElement::toStream(std::ostream& s, unsigned indent) const {
	auto ind = indentStr(indent);
	
	s << ind << "<radialGradient";
	this->Gradient::attribsToStream(s);
	
	if(this->cx.unit != Length::EUnit::PERCENT || this->cx.value != 50){
		s << " cx=\"" << this->cx << "\"";
	}
	
	if(this->cy.unit != Length::EUnit::PERCENT || this->cy.value != 50){
		s << " cy=\"" << this->cy << "\"";
	}
	
	if(this->r.unit != Length::EUnit::PERCENT || this->r.value != 50){
		s << " r=\"" << this->r << "\"";
	}
	
	if(this->fx.unit != Length::EUnit::UNKNOWN){
		s << " fx=\"" << this->fx << "\"";
	}
	
	if(this->fy.unit != Length::EUnit::UNKNOWN){
		s << " fy=\"" << this->fy << "\"";
	}
	
	if(this->children.size() == 0){
		s << "/>";
	}else{
		s << ">" << std::endl;
		this->childrenToStream(s, indent + 1);
		s << ind << "</radialGradient>";
	}
	s << std::endl;
}

void Gradient::attribsToStream(std::ostream& s)const{
	this->Container::attribsToStream(s);
	this->Referencing::attribsToStream(s);
	this->Styleable::attribsToStream(s);
	
	if(this->spreadMethod != ESpreadMethod::DEFAULT){
		s << " spreadMethod=\"" << Gradient::spreadMethodToString(this->spreadMethod) << "\"";
	}
	
	if(this->units != EUnits::OBJECT_BOUNDING_BOX){
		ASSERT(this->units == EUnits::USER_SPACE_ON_USE)
		s << " gradientUnits=\"userSpaceOnUse\"";
	}
	
	if(this->transformations.size() != 0){
		s << " gradientTransform=\"";
		this->transformationsToStream(s);
		s << "\"";
	}
}


void Gradient::StopElement::toStream(std::ostream& s, unsigned indent) const {
	auto ind = indentStr(indent);
	s << ind << "<stop";
	s << " offset=\"" << this->offset << "\"";
	this->Element::attribsToStream(s);
	this->Styleable::attribsToStream(s);
	s << "/>" << std::endl;
}

void Referencing::attribsToStream(std::ostream& s) const {
	if(this->iri.length() == 0 && !this->ref){
		return;
	}
	s << " xlink:href=\"";
	
	if(this->ref){
		s << '#' << this->ref->id;
	}else{
		s << this->iri;
	}
	
	s << "\"";
}

Element* Element::findById(const std::string& elementId) {
	if(this->id == elementId){
		return this;
	}
	return nullptr;
}

Element* Container::findById(const std::string& elementId) {
	if(auto e = this->Element::findById(elementId)){
		return e;
	}
	
	for(auto& c : this->children){
		if(auto e = c->findById(elementId)){
			return e;
		}
	}
	return nullptr;
}

Length Length::make(real value, EUnit unit) noexcept {
	Length ret;
	
	ret.unit = unit;
	ret.value = value;
	
	return ret;
}

void EllipseElement::attribsToStream(std::ostream& s) const {
	this->Shape::attribsToStream(s);
	
	if(this->cx.unit != Length::EUnit::UNKNOWN){
		s << " cx=\"" << this->cx << "\"";
	}
	
	if(this->cy.unit != Length::EUnit::UNKNOWN){
		s << " cy=\"" << this->cy << "\"";
	}
	
	if(this->rx.unit != Length::EUnit::UNKNOWN){
		s << " rx=\"" << this->rx << "\"";
	}
	
	if(this->ry.unit != Length::EUnit::UNKNOWN){
		s << " ry=\"" << this->ry << "\"";
	}
}


void RectElement::attribsToStream(std::ostream& s) const {
	this->Shape::attribsToStream(s);
	this->Rectangle::attribsToStream(s);
	
	if(this->rx.unit != Length::EUnit::UNKNOWN){
		s << " rx=\"" << this->rx << "\"";
	}
	
	if(this->ry.unit != Length::EUnit::UNKNOWN){
		s << " ry=\"" << this->ry << "\"";
	}
}

void RectElement::toStream(std::ostream& s, unsigned indent) const {
	auto ind = indentStr(indent);
	
	s << ind << "<rect";

	this->attribsToStream(s);
	
	s << "/>";
	s << std::endl;
}

void EllipseElement::toStream(std::ostream& s, unsigned indent) const {
	auto ind = indentStr(indent);
	
	s << ind << "<ellipse";

	this->attribsToStream(s);
	
	s << "/>";
	s << std::endl;
}


Gradient::ESpreadMethod Gradient::getSpreadMethod() const noexcept{
	if(this->spreadMethod != ESpreadMethod::DEFAULT){
		return this->spreadMethod;
	}
	
	ASSERT(this->spreadMethod == ESpreadMethod::DEFAULT)
	
	if(this->ref){
		if(auto g = dynamic_cast<Gradient*>(this->ref)){
			return g->getSpreadMethod();
		}
	}
	
	return ESpreadMethod::PAD;
}

const decltype(Container::children)& Gradient::getStops() const noexcept{
	if(this->children.size() != 0){
		return this->children;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<Gradient*>(this->ref)){
			return g->getStops();
		}
	}
	return this->children;
}

Length LinearGradientElement::getX1() const noexcept{
	if(this->x1.unit != Length::EUnit::UNKNOWN){
		return this->x1;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<LinearGradientElement*>(this->ref)){
			return g->getX1();
		}
	}
	return Length::make(0, Length::EUnit::PERCENT);
}

Length LinearGradientElement::getX2() const noexcept{
	if(this->x2.unit != Length::EUnit::UNKNOWN){
		return this->x2;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<LinearGradientElement*>(this->ref)){
			return g->getX2();
		}
	}
	return Length::make(100, Length::EUnit::PERCENT);
}

Length LinearGradientElement::getY1() const noexcept{
	if(this->y1.unit != Length::EUnit::UNKNOWN){
		return this->y1;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<LinearGradientElement*>(this->ref)){
			return g->getY1();
		}
	}
	return Length::make(0, Length::EUnit::PERCENT);
}

Length LinearGradientElement::getY2() const noexcept{
	if(this->y2.unit != Length::EUnit::UNKNOWN){
		return this->y2;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<LinearGradientElement*>(this->ref)){
			return g->getY2();
		}
	}
	return Length::make(0, Length::EUnit::PERCENT);
}

Length RadialGradientElement::getCx() const noexcept{
	if(this->cx.unit != Length::EUnit::UNKNOWN){
		return this->cx;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<RadialGradientElement*>(this->ref)){
			return g->getCx();
		}
	}
	return Length::make(50, Length::EUnit::PERCENT);
}

Length RadialGradientElement::getCy() const noexcept{
	if(this->cy.unit != Length::EUnit::UNKNOWN){
		return this->cy;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<RadialGradientElement*>(this->ref)){
			return g->getCy();
		}
	}
	return Length::make(50, Length::EUnit::PERCENT);
}

Length RadialGradientElement::getR() const noexcept{
	if(this->r.unit != Length::EUnit::UNKNOWN){
		return this->r;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<RadialGradientElement*>(this->ref)){
			return g->getR();
		}
	}
	return Length::make(50, Length::EUnit::PERCENT);
}

Length RadialGradientElement::getFx() const noexcept{
	if(this->fx.unit != Length::EUnit::UNKNOWN){
		return this->fx;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<RadialGradientElement*>(this->ref)){
			return g->getFx();
		}
	}
	return Length::make(0, Length::EUnit::UNKNOWN);
}

Length RadialGradientElement::getFy() const noexcept{
	if(this->fy.unit != Length::EUnit::UNKNOWN){
		return this->fy;
	}
	
	if(this->ref){
		if(auto g = dynamic_cast<RadialGradientElement*>(this->ref)){
			return g->getFy();
		}
	}
	return Length::make(0, Length::EUnit::UNKNOWN);
}
