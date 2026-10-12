#pragma once

#include <string>
#include <vector>

#include <drogon/HttpFilter.h>

namespace soilwatercm::middleware
{

    /**
     * @brief CORS 过滤器
     *
     * 校验 Origin 并补 Access-Control-* 响应头, OPTIONS 预检直接回 204。
     */
    class CorsMiddleware : public drogon::HttpFilter<CorsMiddleware, false>
    {
    public:
        /// @brief 构造, 默认允许全部来源与常用方法/头
        CorsMiddleware();

        /**
         * @brief 处理跨域请求
         *
         * OPTIONS 预检补全 Allow-* 响应头后直接回 204; 其余方法放行到下一环节。
         */
        void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                      drogon::FilterChainCallback&& fccb) override;

        // 以下 setter 配置允许的来源/方法/头、是否带凭证、预检缓存时长

        /// @brief 设置允许的来源列表 ("*" 表示全部)
        void setAllowedOrigins(const std::vector<std::string>& origins);

        /// @brief 设置允许的 HTTP 方法
        void setAllowedMethods(const std::vector<std::string>& methods);

        /// @brief 设置允许的请求头
        void setAllowedHeaders(const std::vector<std::string>& headers);

        /// @brief 是否允许携带凭证
        void setAllowCredentials(bool allow);

        /// @brief 预检结果缓存秒数
        void setMaxAge(int seconds);

    private:
        std::vector<std::string> allowedOrigins_;
        std::vector<std::string> allowedMethods_;
        std::vector<std::string> allowedHeaders_;
        bool allowCredentials_ = true;
        int maxAge_ = 86400;

        /// @brief Origin 是否在允许列表 (支持 "*" 通配)
        bool isOriginAllowed(const std::string& origin);

        std::string joinVector(const std::vector<std::string>& vec, const std::string& delimiter);
    };

}  // namespace soilwatercm::middleware
