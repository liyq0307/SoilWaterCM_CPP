#!/bin/bash

# SoilWaterCM Dependency Installation Script for Linux

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print colored message
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect OS
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_VERSION=$VERSION_ID
    else
        print_error "Cannot detect OS."
        exit 1
    fi
    
    print_info "Detected OS: $OS $OS_VERSION"
}

# Install system dependencies for Ubuntu/Debian
install_ubuntu_deps() {
    print_info "Installing system dependencies for Ubuntu/Debian..."
    
    sudo apt-get update
    
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config \
        curl \
        zip \
        unzip \
        tar \
        ninja-build \
        postgresql \
        postgresql-contrib \
        postgresql-14-postgis-3 \
        mongodb \
        redis-server \
        libgdal-dev \
        gdal-bin \
        python3 \
        python3-pip \
        libssl-dev \
        libcurl4-openssl-dev
    
    print_info "System dependencies installed."
}

# Install system dependencies for CentOS/RHEL
install_centos_deps() {
    print_info "Installing system dependencies for CentOS/RHEL..."
    
    sudo yum install -y epel-release
    
    sudo yum install -y \
        gcc \
        gcc-c++ \
        make \
        cmake \
        git \
        pkgconfig \
        curl \
        zip \
        unzip \
        tar \
        ninja-build \
        postgresql \
        postgresql-contrib \
        postgis \
        mongodb \
        redis \
        gdal-devel \
        gdal \
        python3 \
        python3-pip \
        openssl-devel \
        libcurl-devel
    
    print_info "System dependencies installed."
}

# Install vcpkg
install_vcpkg() {
    print_info "Installing vcpkg..."
    
    local VCPKG_DIR="${PROJECT_DIR}/vcpkg"
    
    if [ -d "$VCPKG_DIR" ]; then
        print_warn "vcpkg already exists at $VCPKG_DIR"
        read -p "Reinstall? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf "$VCPKG_DIR"
        else
            print_info "Skipping vcpkg installation."
            return
        fi
    fi
    
    git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_DIR"
    cd "$VCPKG_DIR"
    ./bootstrap-vcpkg.sh
    
    print_info "vcpkg installed at $VCPKG_DIR"
    print_info "Set environment variable: export VCPKG_ROOT=$VCPKG_DIR"
}

# Install vcpkg packages
install_vcpkg_packages() {
    print_info "Installing vcpkg packages..."
    
    local VCPKG_ROOT="${VCPKG_ROOT:-${PROJECT_DIR}/vcpkg}"
    
    if [ ! -d "$VCPKG_ROOT" ]; then
        print_error "vcpkg not found. Please install vcpkg first."
        exit 1
    fi
    
    cd "$VCPKG_ROOT"
    
    # Install packages
    ./vcpkg install \
        drogon:x64-linux \
        nlohmann-json:x64-linux \
        spdlog:x64-linux \
        pqxx:x64-linux \
        mongo-cxx-driver:x64-linux \
        hiredis:x64-linux \
        jwt-cpp:x64-linux \
        gtest:x64-linux \
        benchmark:x64-linux \
        yaml-cpp:x64-linux \
        openssl:x64-linux \
        curl:x64-linux \
        zlib:x64-linux
    
    print_info "vcpkg packages installed."
}

# Install Python dependencies
install_python_deps() {
    print_info "Installing Python dependencies..."
    
    pip3 install --user numpy scipy matplotlib
    
    print_info "Python dependencies installed."
}

# Install external tools (optional)
install_external_tools() {
    print_info "Installing external tools (optional)..."
    
    # QGIS (with GRASS and SAGA)
    read -p "Install QGIS (with GRASS and SAGA)? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [ "$OS" = "ubuntu" ] || [ "$OS" = "debian" ]; then
            sudo apt-get install -y qgis qgis-plugin-grass saga
        elif [ "$OS" = "centos" ] || [ "$OS" = "rhel" ]; then
            sudo yum install -y qgis grass saga
        fi
        print_info "QGIS installed."
    fi
    
    # OTB
    read -p "Install OTB (Orfeo Toolbox)? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Please download and install OTB manually from: https://www.orfeo-toolbox.org/download/"
    fi
    
    # MintPy
    read -p "Install MintPy (for InSAR time series analysis)? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Installing MintPy..."
        
        # Check if conda is installed
        if ! command -v conda &> /dev/null; then
            print_warn "conda not found. Please install Miniconda or Anaconda first."
            print_info "Download from: https://docs.conda.io/en/latest/miniconda.html"
        else
            conda create -n mintpy python=3.9 -y
            conda activate mintpy
            conda install -c conda-forge mintpy -y
            pip install pyaps3
            conda deactivate
            print_info "MintPy installed in conda environment 'mintpy'."
        fi
    fi
    
    print_info "External tools installation completed."
}

# Configure databases
configure_databases() {
    print_info "Configuring databases..."
    
    # PostgreSQL
    read -p "Configure PostgreSQL? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Configuring PostgreSQL..."
        
        sudo systemctl start postgresql
        sudo systemctl enable postgresql
        
        read -p "PostgreSQL user name [soilwatercm]: " PG_USER
        PG_USER=${PG_USER:-soilwatercm}
        
        read -sp "PostgreSQL password: " PG_PASSWORD
        echo
        
        sudo -u postgres psql << EOF
CREATE USER $PG_USER WITH PASSWORD '$PG_PASSWORD';
CREATE DATABASE soilwatercm OWNER $PG_USER;
GRANT ALL PRIVILEGES ON DATABASE soilwatercm TO $PG_USER;
\c soilwatercm
CREATE EXTENSION IF NOT EXISTS postgis;
CREATE EXTENSION IF NOT EXISTS postgis_topology;
EOF
        
        print_info "PostgreSQL configured."
    fi
    
    # MongoDB
    read -p "Configure MongoDB? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Configuring MongoDB..."
        
        sudo systemctl start mongodb
        sudo systemctl enable mongodb
        
        print_info "MongoDB configured."
    fi
    
    # Redis
    read -p "Configure Redis? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Configuring Redis..."
        
        sudo systemctl start redis
        sudo systemctl enable redis
        
        read -p "Set Redis password? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            read -sp "Redis password: " REDIS_PASSWORD
            echo
            echo "requirepass $REDIS_PASSWORD" | sudo tee -a /etc/redis/redis.conf
            sudo systemctl restart redis
        fi
        
        print_info "Redis configured."
    fi
    
    print_info "Databases configuration completed."
}

# Print usage
print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  --system            Install system dependencies only"
    echo "  --vcpkg             Install vcpkg only"
    echo "  --packages          Install vcpkg packages only"
    echo "  --python            Install Python dependencies only"
    echo "  --tools             Install external tools only"
    echo "  --db                Configure databases only"
    echo "  --all               Install everything (default)"
    echo ""
    echo "Examples:"
    echo "  $0                  # Install everything"
    echo "  $0 --system         # Install system dependencies only"
    echo "  $0 --vcpkg          # Install vcpkg only"
}

# Main function
main() {
    local install_system=false
    local install_vcpkg_flag=false
    local install_packages=false
    local install_python=false
    local install_tools=false
    local configure_db=false
    local install_all=true
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                print_usage
                exit 0
                ;;
            --system)
                install_system=true
                install_all=false
                shift
                ;;
            --vcpkg)
                install_vcpkg_flag=true
                install_all=false
                shift
                ;;
            --packages)
                install_packages=true
                install_all=false
                shift
                ;;
            --python)
                install_python=true
                install_all=false
                shift
                ;;
            --tools)
                install_tools=true
                install_all=false
                shift
                ;;
            --db)
                configure_db=true
                install_all=false
                shift
                ;;
            --all)
                install_all=true
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done
    
    print_info "SoilWaterCM Dependency Installation"
    
    detect_os
    
    if [ "$install_all" = true ] || [ "$install_system" = true ]; then
        if [ "$OS" = "ubuntu" ] || [ "$OS" = "debian" ]; then
            install_ubuntu_deps
        elif [ "$OS" = "centos" ] || [ "$OS" = "rhel" ]; then
            install_centos_deps
        else
            print_error "Unsupported OS: $OS"
            exit 1
        fi
    fi
    
    if [ "$install_all" = true ] || [ "$install_vcpkg_flag" = true ]; then
        install_vcpkg
    fi
    
    if [ "$install_all" = true ] || [ "$install_packages" = true ]; then
        install_vcpkg_packages
    fi
    
    if [ "$install_all" = true ] || [ "$install_python" = true ]; then
        install_python_deps
    fi
    
    if [ "$install_all" = true ] || [ "$install_tools" = true ]; then
        install_external_tools
    fi
    
    if [ "$install_all" = true ] || [ "$configure_db" = true ]; then
        configure_databases
    fi
    
    print_info "Dependency installation completed!"
}

main "$@"
