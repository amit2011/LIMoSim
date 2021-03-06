#include "osmdocument.h"

namespace LIMoSim
{

OSMDocument::OSMDocument() :
    p_map(Map::getInstance()),
    useWgs(true)
{

}

OSMDocument OSMDocument::fromXML(DOMElement *_entry)
{
    OSMDocument document;

    std::string generator = _entry->getAttribute("generator").toString();
    if(generator=="LIMoSim")
        document.useWgs = false;

    document.createOSMEntries(_entry);
    document.adjustNodePositions();

    if(document.useWgs)
    {
        document.createRelations();
        //document.createWays();
    }
    else
        document.createWays();


    return document;
}

void OSMDocument::createOSMEntries(DOMElement *_entry)
{
    for(unsigned int i=0; i<_entry->childNodes.size(); i++)
    {
        DOMElement *element = _entry->childNodes.at(i)->toElement();
        std::string name = element->tagName;

        if(name=="node")
        {
            OSMNodeEntry entry = OSMNodeEntry::fromXML(element, this);
            std::stringstream id; id<<entry.id;

            m_nodes[id.str()] = entry;
        }
        else if(name=="relation")
        {
            OSMRelationEntry entry = OSMRelationEntry::fromXML(element, this);
            if(entry.type=="associatedStreet")
            {

            }
            m_relations.push_back(entry);
        }
        else if(name=="way")
        {
            OSMWayEntry entry = OSMWayEntry::fromXML(element, this);
            m_ways[entry.id] = entry;
        }
        else if(name=="controller")
        {
           // parseController(element);
        }
    }
}

void OSMDocument::adjustNodePositions()
{
    std::cout << "OSMDocument::adjustNodePositions " << useWgs << "\tn: " << m_nodes.size() << std::endl;

    if(useWgs) // TODO: rel getNodeEntries
    {
        for(unsigned int i=0; i<m_relations.size(); i++)
        {
            OSMRelationEntry &relation = m_relations.at(i);
            for(unsigned int w=0; w<relation.streets.size(); w++)
            {
                std::string wayId = relation.streets.at(w);

                if(hasWay(wayId))
                {
                    OSMWayEntry way = m_ways[wayId];
                    for(unsigned int n=0; n<way.nodes.size(); n++)
                    {
                        std::string nodeId = way.nodes.at(n);
                        if(hasNode(nodeId))
                        {
                            OSMNodeEntry node = m_nodes[nodeId];
                            adjustBounds(node);
                        }


                    }
                }

            }
        }
    }



/*
    std::map<std::string, OSMNodeEntry>::iterator n;
    for(n=m_nodes.begin(); n!=m_nodes.end(); n++)
    {
        OSMNodeEntry entry = n->second;
        adjustBounds(entry);
    }*/





    Position bottomLeft(m_bounds.minLon, m_bounds.minLat);
    Position topRight(m_bounds.maxLon, m_bounds.maxLat);
    Vector3d offset = m_wgs.getOffset(topRight, bottomLeft);
    Vector3d s = (topRight-bottomLeft);

    std::cout << "wgs: " << offset.toString() << "\tcartesian: " << s.toString() << std::endl;
    std::cout << "DX: " << bottomLeft.toString() << "\t" << topRight.toString() << std::endl;


    // nodes preprocessing
    std::map<std::string, OSMNodeEntry>::iterator it;
    for(it=m_nodes.begin(); it!=m_nodes.end(); it++)
    {
        OSMNodeEntry &_entry = it->second;
        if(useWgs)
        {
            Vector3d offset = m_wgs.getOffset(Position(_entry.lon, _entry.lat), m_bounds.getOrigin());
            _entry.position = offset;
            _entry.position = _entry.position + Vector3d(100, 10);
        }
        else
            _entry.position = Position(_entry.x, _entry.y);

    }

    std::cout << "n: " << p_map->getNodes().size() << "\tw: " << p_map->getWays().size() << std::endl;
    std::cout << "osm: n: " << m_nodes.size() << "\tw: " << m_ways.size() << std::endl;
}

void OSMDocument::adjustBounds(const OSMNodeEntry &_node)
{
    if(_node.lat<m_bounds.minLat)
        m_bounds.minLat = _node.lat;
    if(_node.lat>m_bounds.maxLat)
        m_bounds.maxLat = _node.lat;
    if(_node.lon<m_bounds.minLon)
        m_bounds.minLon = _node.lon;
    if(_node.lon>m_bounds.maxLon)
        m_bounds.maxLon = _node.lon;
}

void OSMDocument::createRelations()
{
    std::cout << "OSMDocument::createRelations" << std::endl;

    for(unsigned int i=0; i<m_relations.size(); i++)
    {
        OSMRelationEntry entry = m_relations.at(i);
       // std::cout << "relation: " << entry.name << std::endl;

        for(unsigned int w=0; w<entry.streets.size(); w++)
        {
            std::string wayId = entry.streets.at(w);

            if(hasWay(wayId))
            {
                OSMWayEntry wayEntry = m_ways[wayId];

                std::string type = wayEntry.highway;
                if(!(type=="path" || type=="service" || type=="steps" || type=="pedestrian" || type=="footway"))
                {
                    Way *way = p_map->getWay(wayEntry.id);
                    if(!way)
                        way = wayEntry.toWay(m_nodes);
                }
            }


        }
    }
}

void OSMDocument::createWays()
{
    std::map<std::string, OSMWayEntry>::iterator it;
    for(it=m_ways.begin(); it!=m_ways.end(); it++)
    {
        OSMWayEntry entry = it->second;

        std::string type = entry.highway;
        if(!(type=="path" || type=="service" || type=="steps" || type=="pedestrian" || type=="footway"))
        {
            if(entry.highway=="primary" || entry.highway=="residential")
            {
                Way *way = p_map->getWay(entry.id);
                if(!way)
                    way = entry.toWay(m_nodes);
            }

        }
    }
}

bool OSMDocument::hasWay(const std::string &_id)
{
    return (m_ways.count(_id)>0);
}

bool OSMDocument::hasNode(const std::string &_id)
{
    return (m_nodes.count(_id)>0);
}

DOMElement* OSMDocument::toXML()
{
    DOMElement *xml = new DOMElement("osm");
    xml->setAttribute("generator", Variant("LIMoSim"));

    // nodes
    std::map<std::string,Node*> nodes = p_map->getNodes();
    std::map<std::string,Node*>::iterator n;
    for(n=nodes.begin(); n!=nodes.end(); n++)
    {
        Node *node = n->second;
        OSMNodeEntry entry = OSMNodeEntry::fromNode(node, this);

        xml->appendChild(entry.toXML());
    }

    // ways
    std::map<std::string,Way*>& ways = p_map->getWays();
    std::map<std::string,Way*>::iterator w;
    for(w=ways.begin(); w!=ways.end(); w++)
    {
        Way *way = w->second;
        OSMWayEntry entry = OSMWayEntry::fromWay(way, this);

        xml->appendChild(entry.toXML());
    }



    return xml;
}

}

