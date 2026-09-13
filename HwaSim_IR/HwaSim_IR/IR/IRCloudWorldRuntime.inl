LMatrix4f HwaSimIR::CloudWorldRenderMatrix() const {
    if(!m_cloudFrameReady||m_p6.enabled)return LMatrix4f::ident_mat();
    auto x=m_cloudFrame.worldVectorToLocal({1,0,0});
    auto y=m_cloudFrame.worldVectorToLocal({0,1,0});
    auto z=m_cloudFrame.worldVectorToLocal({0,0,1});
    auto p=m_cloudFrame.toLocal({0,0,0});
    return LMatrix4f(float(x.x),float(x.y),float(x.z),0,
        float(y.x),float(y.y),float(y.z),0,float(z.x),float(z.y),float(z.z),0,
        float(p.x),float(p.y),float(p.z),1);
}
LPoint3f HwaSimIR::CloudRenderToWorld(const LPoint3f& p) const {
    if(!m_cloudFrameReady||m_p6.enabled)return p;
    auto q=m_cloudFrame.toWorld({p[0],p[1],p[2]});
    return LPoint3f(float(q.x),float(q.y),float(q.z));
}
void HwaSimIR::RefreshCloudWorldFrame() {
    const auto transform=CloudWorldRenderMatrix();
    if(!m_cloudVolumeWorldRoot.is_empty())m_cloudVolumeWorldRoot.set_mat(transform);
    if(!m_cloudSheetWorldRoot.is_empty())m_cloudSheetWorldRoot.set_mat(transform);
    LMatrix4f inverse;inverse.invert_from(transform);
    for(auto& cloud:m_cloudNodes){
        cloud.set_shader_input("u_cloud_render_to_world",inverse);
    }
}
void HwaSimIR::AuditCloudDescriptor(const IRWorldCloudDescriptor& d) const {
    if(m_p6.enabled||!m_cloudAppearance.enabled||!std::getenv("WorldCloudAudit"))return;
    std::ostringstream s;s<<std::setprecision(17);
    const auto& t=m_cloudAppearance.templates.at(d.densityTemplate);
    s<<"[CloudWorldDescriptor] {\"cloudId\":\""<<IRWorldCloudStreaming::cloudIdText(d.cloudId)<<"\",\"cell\":["<<d.cellX<<","<<d.cellY
        <<"],\"position\":["<<d.worldX<<","<<d.worldY<<","<<d.worldZ<<"],\"radius\":["<<d.radiusX<<","<<d.radiusY<<","<<d.radiusZ
        <<"],\"density\":"<<d.density<<",\"rotation\":"<<d.rotationDeg<<",\"template\":\""<<t.key<<"\",\"sha256\":\""<<t.sha256
        <<"\",\"animation\":\"static\"}";
    std::cout<<s.str()<<std::endl;
}
